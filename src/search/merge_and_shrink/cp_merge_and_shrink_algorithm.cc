#include "cp_merge_and_shrink_algorithm.h"

#include "cost_partitioning.h"
#include "distances.h"
#include "factored_transition_system.h"
#include "fts_factory.h"
#include "label_reduction.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "merge_strategy.h"
#include "merge_strategy_factory.h"
#include "shrink_strategy.h"
#include "transition_system.h"
#include "types.h"
#include "utils.h"

#include "../options/option_parser.h"
#include "../options/options.h"

#include "../task_utils/task_properties.h"

#include "../utils/countdown_timer.h"
#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/math.h"
#include "../utils/system.h"
#include "../utils/timer.h"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using options::Bounds;
using options::OptionParser;
using options::Options;
using utils::ExitCode;

namespace merge_and_shrink {
static void log_progress(const utils::Timer &timer, string msg) {
    cout << "M&S algorithm timer: " << timer << " (" << msg << ")" << endl;
}

CPMergeAndShrinkAlgorithm::CPMergeAndShrinkAlgorithm(const Options &opts) :
    merge_strategy_factory(opts.get<shared_ptr<MergeStrategyFactory>>("merge_strategy")),
    shrink_strategy(opts.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
    label_reduction(opts.get<shared_ptr<LabelReduction>>("label_reduction", nullptr)),
    cp_factory(opts.get<shared_ptr<CostPartitioningFactory>>("cost_partitioning", nullptr)),
    max_states(opts.get<int>("max_states")),
    max_states_before_merge(opts.get<int>("max_states_before_merge")),
    shrink_threshold_before_merge(opts.get<int>("threshold_before_merge")),
    prune_unreachable_states(opts.get<bool>("prune_unreachable_states")),
    prune_irrelevant_states(opts.get<bool>("prune_irrelevant_states")),
    verbosity(static_cast<utils::Verbosity>(opts.get_enum("verbosity"))),
    main_loop_max_time(opts.get<double>("main_loop_max_time")),
    atomic_label_reduction(opts.get<bool>("atomic_label_reduction")),
    compute_atomic_snapshot(opts.get<bool>("compute_atomic_snapshot")),
    compute_final_snapshot(opts.get<bool>("compute_final_snapshot")),
    main_loop_target_num_snapshots(opts.get<int>("main_loop_target_num_snapshots")),
    main_loop_snapshot_each_iteration(opts.get<int>("main_loop_snapshot_each_iteration")),
    snapshot_moment(static_cast<SnapshotMoment>(opts.get_enum("snapshot_moment"))),
    filter_trivial_factors(opts.get<bool>("filter_trivial_factors")),
    starting_peak_memory(0) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
    assert(shrink_threshold_before_merge <= max_states_before_merge);
}

void CPMergeAndShrinkAlgorithm::report_peak_memory_delta(bool final) const {
    if (final)
        cout << "Final";
    else
        cout << "Current";
    cout << " peak memory increase of merge-and-shrink algorithm: "
         << utils::get_peak_memory_in_kb() - starting_peak_memory << " KB"
         << endl;
}

void CPMergeAndShrinkAlgorithm::dump_options() const {
    if (verbosity >= utils::Verbosity::VERBOSE) {
        if (merge_strategy_factory) { // deleted after merge strategy extraction
            merge_strategy_factory->dump_options();
            cout << endl;
        }

        cout << "Options related to size limits and shrinking: " << endl;
        cout << "Transition system size limit: " << max_states << endl
             << "Transition system size limit right before merge: "
             << max_states_before_merge << endl;
        cout << "Threshold to trigger shrinking right before merge: "
             << shrink_threshold_before_merge << endl;
        cout << endl;

        shrink_strategy->dump_options();
        cout << endl;

        if (label_reduction) {
            label_reduction->dump_options();
        } else {
            cout << "Label reduction disabled" << endl;
        }
        cout << endl;
    }
}

void CPMergeAndShrinkAlgorithm::warn_on_unusual_options() const {
    string dashes(79, '=');
    if (!label_reduction) {
        cout << dashes << endl
             << "WARNING! You did not enable label reduction.\nThis may "
            "drastically reduce the performance of merge-and-shrink!"
             << endl << dashes << endl;
    } else if (label_reduction->reduce_before_merging() && label_reduction->reduce_before_shrinking()) {
        cout << dashes << endl
             << "WARNING! You set label reduction to be applied twice in each merge-and-shrink\n"
            "iteration, both before shrinking and merging. This double computation effort\n"
            "does not pay off for most configurations!"
             << endl << dashes << endl;
    } else {
        if (label_reduction->reduce_before_shrinking() &&
            (shrink_strategy->get_name() == "f-preserving"
             || shrink_strategy->get_name() == "random")) {
            cout << dashes << endl
                 << "WARNING! Bucket-based shrink strategies such as f-preserving random perform\n"
                "best if used with label reduction before merging, not before shrinking!"
                 << endl << dashes << endl;
        }
        if (label_reduction->reduce_before_merging() &&
            shrink_strategy->get_name() == "bisimulation") {
            cout << dashes << endl
                 << "WARNING! Shrinking based on bisimulation performs best if used with label\n"
                "reduction before shrinking, not before merging!"
                 << endl << dashes << endl;
        }
    }

    if (!prune_unreachable_states || !prune_irrelevant_states) {
        cout << dashes << endl
             << "WARNING! Pruning is (partially) turned off!\nThis may "
            "drastically reduce the performance of merge-and-shrink!"
             << endl << dashes << endl;
    }
}

bool CPMergeAndShrinkAlgorithm::ran_out_of_time(
    const utils::CountdownTimer &timer) const {
    if (timer.is_expired()) {
        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << "Ran out of time, stopping computation." << endl;
            cout << endl;
        }
        return true;
    }
    return false;
}

class NextSnapshot {
private:
    const double max_time;
    const int max_iterations;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;
    const utils::Verbosity verbosity;

    double next_time_to_compute_snapshot;
    int next_iteration_to_compute_snapshot;
    int num_main_loop_snapshots;

    void compute_next_snapshot_time(double current_time) {
        int num_remaining_scp_heuristics = main_loop_target_num_snapshots - num_main_loop_snapshots;
        // safeguard against having aimed_num_scp_heuristics = 0
        if (num_remaining_scp_heuristics <= 0) {
            next_time_to_compute_snapshot = max_time + 1.0;
            return;
        }
        double remaining_time = max_time - current_time;
        if (remaining_time <= 0.0) {
            next_time_to_compute_snapshot = current_time;
            return;
        }
        double time_offset = remaining_time / static_cast<double>(num_remaining_scp_heuristics);
        next_time_to_compute_snapshot = current_time + time_offset;
    }

    void compute_next_snapshot_iteration(int current_iteration) {
        if (main_loop_target_num_snapshots) {
            int num_remaining_scp_heuristics = main_loop_target_num_snapshots - num_main_loop_snapshots;
            // safeguard against having aimed_num_scp_heuristics = 0
            if (num_remaining_scp_heuristics <= 0) {
                next_iteration_to_compute_snapshot = max_iterations + 1;
                return;
            }
            int num_remaining_iterations = max_iterations - current_iteration;
            if (!num_remaining_iterations || num_remaining_scp_heuristics >= num_remaining_iterations) {
                next_iteration_to_compute_snapshot = current_iteration + 1;
                return;
            }
            double iteration_offset = num_remaining_iterations / static_cast<double>(num_remaining_scp_heuristics);
            assert(iteration_offset >= 1.0);
            next_iteration_to_compute_snapshot = current_iteration + static_cast<int>(iteration_offset);
        } else {
            next_iteration_to_compute_snapshot = current_iteration + main_loop_snapshot_each_iteration;
        }
    }
public:
    NextSnapshot(
        double max_time,
        int max_iterations,
        int main_loop_target_num_snapshots,
        int main_loop_snapshot_each_iteration,
        utils::Verbosity verbosity)
        : max_time(max_time),
          max_iterations(max_iterations),
          main_loop_target_num_snapshots(main_loop_target_num_snapshots),
          main_loop_snapshot_each_iteration(main_loop_snapshot_each_iteration),
          verbosity(verbosity),
          num_main_loop_snapshots(0) {
        assert(main_loop_target_num_snapshots || main_loop_snapshot_each_iteration);
        assert(!main_loop_target_num_snapshots || !main_loop_snapshot_each_iteration);
        compute_next_snapshot_time(0);
        compute_next_snapshot_iteration(0);
        if (verbosity == utils::Verbosity::DEBUG) {
            cout << "Snapshot collector: next time: " << next_time_to_compute_snapshot
                 << ", next iteration: " << next_iteration_to_compute_snapshot
                 << endl;
        }
    }

    bool compute_next_snapshot(double current_time, int current_iteration) {
        if (!main_loop_target_num_snapshots && !main_loop_snapshot_each_iteration) {
            return false;
        }
        if (verbosity == utils::Verbosity::DEBUG) {
            cout << "Snapshot collector: compute next snapshot? current time: " << current_time
                 << ", current iteration: " << current_iteration
                 << ", num existing heuristics: " << num_main_loop_snapshots
                 << endl;
        }
        bool compute = false;
        if (current_time >= next_time_to_compute_snapshot ||
            current_iteration >= next_iteration_to_compute_snapshot) {
            compute = true;
        }
        if (compute) {
            compute_next_snapshot_time(current_time);
            compute_next_snapshot_iteration(current_iteration);
            if (verbosity == utils::Verbosity::DEBUG) {
                cout << "Compute snapshot now" << endl;
                cout << "Next snapshot: next time: " << next_time_to_compute_snapshot
                     << ", next iteration: " << next_iteration_to_compute_snapshot
                     << endl;
            }
            ++num_main_loop_snapshots;
        }
        return compute;
    }
};

bool CPMergeAndShrinkAlgorithm::main_loop(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy,
    vector<unique_ptr<CostPartitioning>> &cost_partitionings) {
    utils::CountdownTimer timer(main_loop_max_time);
    if (verbosity >= utils::Verbosity::NORMAL) {
        cout << "Starting main loop ";
        if (main_loop_max_time == numeric_limits<double>::infinity()) {
            cout << "without a time limit." << endl;
        } else {
            cout << "with a time limit of "
                 << main_loop_max_time << "s." << endl;
        }
    }
    int maximum_intermediate_size = 0;
    for (int i = 0; i < fts.get_size(); ++i) {
        int size = fts.get_transition_system(i).get_size();
        if (size > maximum_intermediate_size) {
            maximum_intermediate_size = size;
        }
    }

    unique_ptr<MergeStrategy> merge_strategy =
        merge_strategy_factory->compute_merge_strategy(task_proxy, fts);
    merge_strategy_factory = nullptr;

    auto log_main_loop_progress = [&timer](const string &msg) {
            cout << "M&S algorithm main loop timer: "
                 << timer.get_elapsed_time()
                 << " (" << msg << ")" << endl;
        };
    int iteration_counter = 0;
    unique_ptr<NextSnapshot> next_snapshot = nullptr;
    if (main_loop_target_num_snapshots || main_loop_snapshot_each_iteration) {
        next_snapshot = utils::make_unique_ptr<NextSnapshot>(
        main_loop_max_time,
        fts.get_num_active_entries() - 1,
        main_loop_target_num_snapshots,
        main_loop_snapshot_each_iteration,
        verbosity);
    }
    bool computed_snapshot_after_last_transformation = false;
    while (fts.get_num_active_entries() > 1) {
        ++iteration_counter;
        // Choose next transition systems to merge
        pair<int, int> merge_indices = merge_strategy->get_next();
        if (ran_out_of_time(timer)) {
            break;
        }
        int merge_index1 = merge_indices.first;
        int merge_index2 = merge_indices.second;
        assert(merge_index1 != merge_index2);
        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << "Next pair of indices: ("
                 << merge_index1 << ", " << merge_index2 << ")" << endl;
            if (verbosity >= utils::Verbosity::VERBOSE) {
                fts.statistics(merge_index1);
                fts.statistics(merge_index2);
            }
            log_main_loop_progress("after computation of next merge");
        }

        // Label reduction (before shrinking)
        if (label_reduction && label_reduction->reduce_before_shrinking()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, verbosity);
            if (reduced) {
                computed_snapshot_after_last_transformation = false;
            }
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_main_loop_progress("after label reduction");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_LABEL_REDUCTION &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter + 1)) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            computed_snapshot_after_last_transformation = true;
            log_main_loop_progress("after handling main loop snapshot");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Shrinking
        bool shrunk = shrink_before_merge_step(
            fts,
            merge_index1,
            merge_index2,
            max_states,
            max_states_before_merge,
            shrink_threshold_before_merge,
            *shrink_strategy,
            verbosity);
        if (shrunk) {
            computed_snapshot_after_last_transformation = false;
        }
        if (verbosity >= utils::Verbosity::NORMAL && shrunk) {
            log_main_loop_progress("after shrinking");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_SHRINKING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter + 1)) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            computed_snapshot_after_last_transformation = true;
            log_main_loop_progress("after handling main loop snapshot");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Label reduction (before merging)
        if (label_reduction && label_reduction->reduce_before_merging()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, verbosity);
            if (reduced) {
                computed_snapshot_after_last_transformation = false;
            }
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_main_loop_progress("after label reduction");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Merging
        int merged_index = fts.merge(merge_index1, merge_index2, verbosity);
        int abs_size = fts.get_transition_system(merged_index).get_size();
        if (abs_size > maximum_intermediate_size) {
            maximum_intermediate_size = abs_size;
        }

        if (verbosity >= utils::Verbosity::NORMAL) {
            if (verbosity >= utils::Verbosity::VERBOSE) {
                fts.statistics(merged_index);
            }
            log_main_loop_progress("after merging");
        }

        computed_snapshot_after_last_transformation = false;
        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_MERGING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter + 1)) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            computed_snapshot_after_last_transformation = true;
            log_main_loop_progress("after handling main loop snapshot");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Pruning
        if (prune_unreachable_states || prune_irrelevant_states) {
            bool pruned = prune_step(
                fts,
                merged_index,
                prune_unreachable_states,
                prune_irrelevant_states,
                verbosity);
            if (pruned) {
                computed_snapshot_after_last_transformation = false;
            }
            if (verbosity >= utils::Verbosity::NORMAL && pruned) {
                if (verbosity >= utils::Verbosity::VERBOSE) {
                    fts.statistics(merged_index);
                }
                log_main_loop_progress("after pruning");
            }
        }

        /*
          NOTE: both the shrink strategy classes and the construction
          of the composite transition system require the input
          transition systems to be non-empty, i.e. the initial state
          not to be pruned/not to be evaluated as infinity.
        */
        if (!fts.is_factor_solvable(merged_index)) {
            if (verbosity >= utils::Verbosity::NORMAL) {
                cout << "Abstract problem is unsolvable, stopping "
                    "computation. " << endl << endl;
            }
            vector<unique_ptr<CostPartitioning>>().swap(cost_partitionings);
            cost_partitionings.reserve(1);
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts, merged_index), verbosity));
            computed_snapshot_after_last_transformation = true;
            break;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_PRUNING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter + 1)) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            computed_snapshot_after_last_transformation = true;
            log_main_loop_progress("after handling main loop snapshot");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // End-of-iteration output.
        if (verbosity >= utils::Verbosity::VERBOSE) {
            report_peak_memory_delta();
        }
        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << endl;
        }
    }

    cout << "End of merge-and-shrink algorithm, statistics:" << endl;
    cout << "Main loop runtime: " << timer.get_elapsed_time() << endl;
    cout << "Maximum intermediate abstraction size: "
         << maximum_intermediate_size << endl;
    shrink_strategy = nullptr;
    label_reduction = nullptr;
    return computed_snapshot_after_last_transformation;
}

vector<unique_ptr<Abstraction>> CPMergeAndShrinkAlgorithm::compute_abstractions_over_fts(
    FactoredTransitionSystem &fts, int unsolvable_index) const {
    vector<unique_ptr<Abstraction>> abstractions;
    if (unsolvable_index == -1) {
        vector<int> active_nontrivial_factor_indices;
        active_nontrivial_factor_indices.reserve(fts.get_num_active_entries());
        for (int index : fts) {
            if (!filter_trivial_factors || !fts.is_factor_trivial(index)) {
                active_nontrivial_factor_indices.push_back(index);
            }
        }
        assert(!active_nontrivial_factor_indices.empty());

        for (int index : active_nontrivial_factor_indices) {
            TransitionSystem *transition_system = fts.get_transition_system_raw_ptr(index);
            unique_ptr<MergeAndShrinkRepresentation> mas_representation = nullptr;
            if (dynamic_cast<const MergeAndShrinkRepresentationLeaf *>(fts.get_mas_representation_raw_ptr(index))) {
                mas_representation = utils::make_unique_ptr<MergeAndShrinkRepresentationLeaf>(
                    dynamic_cast<const MergeAndShrinkRepresentationLeaf *>
                        (fts.get_mas_representation_raw_ptr(index)));
            } else {
                mas_representation = utils::make_unique_ptr<MergeAndShrinkRepresentationMerge>(
                    dynamic_cast<const MergeAndShrinkRepresentationMerge *>(
                        fts.get_mas_representation_raw_ptr(index)));
            }
            abstractions.push_back(utils::make_unique_ptr<Abstraction>(transition_system, move(mas_representation)));
        }
    } else {
        auto factor = fts.extract_ts_and_representation(unsolvable_index);
        abstractions.push_back(utils::make_unique_ptr<Abstraction>(factor.first.release(), move(factor.second)));
    }
    return abstractions;
}

vector<unique_ptr<CostPartitioning>> CPMergeAndShrinkAlgorithm::compute_ms_cps(
    const TaskProxy &task_proxy) {
    if (starting_peak_memory) {
        cerr << "Calling compute_ms_cps twice is not "
             << "supported!" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    starting_peak_memory = utils::get_peak_memory_in_kb();

    utils::Timer timer;
    cout << "Running merge-and-shrink algorithm..." << endl;
    task_properties::verify_no_axioms(task_proxy);
    dump_options();
    warn_on_unusual_options();
    cout << endl;

    const bool compute_init_distances =
        shrink_strategy->requires_init_distances() ||
        merge_strategy_factory->requires_init_distances() ||
        prune_unreachable_states;
    const bool compute_goal_distances =
        shrink_strategy->requires_goal_distances() ||
        merge_strategy_factory->requires_goal_distances() ||
        prune_irrelevant_states;
    FactoredTransitionSystem fts =
        create_factored_transition_system(
            task_proxy,
            compute_init_distances,
            compute_goal_distances,
            verbosity);
    if (verbosity >= utils::Verbosity::NORMAL) {
        log_progress(timer, "after computation of atomic factors");
    }

    vector<unique_ptr<CostPartitioning>> cost_partitionings;

    /*
      Prune all atomic factors according to the chosen options. Stop early if
      one factor is unsolvable.

      TODO: think about if we can prune already while creating the atomic FTS.
    */
    bool pruned = false;
    bool unsolvable = false;
    for (int index = 0; index < fts.get_size(); ++index) {
        assert(fts.is_active(index));
        if (prune_unreachable_states || prune_irrelevant_states) {
            bool pruned_factor = prune_step(
                fts,
                index,
                prune_unreachable_states,
                prune_irrelevant_states,
                verbosity);
            pruned = pruned || pruned_factor;
        }
        if (!fts.is_factor_solvable(index)) {
            cout << "Atomic FTS is unsolvable, stopping computation." << endl;
            unsolvable = true;
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts, index), verbosity));
            break;
        }
    }
    if (verbosity >= utils::Verbosity::NORMAL) {
        if (pruned) {
            log_progress(timer, "after pruning atomic factors");
        }
    }

    if (!unsolvable) {
        if (label_reduction) {
            label_reduction->initialize(task_proxy);
        }

        if (label_reduction && atomic_label_reduction) {
            bool reduced = label_reduction->reduce(pair<int, int>(-1, -1), fts, verbosity);
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_progress(timer, "after label reduction on atomic FTS");
            }
        }

        if (compute_atomic_snapshot) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling atomic snapshot");
            }
        }

        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << endl;
        }

        bool computed_snapshot_after_last_transformation = false;
        if (main_loop_max_time > 0) {
            computed_snapshot_after_last_transformation = main_loop(fts, task_proxy, cost_partitionings);
        }

        if (computed_snapshot_after_last_transformation) {
            assert(!cost_partitionings.empty());
        }

        if ((compute_final_snapshot && !computed_snapshot_after_last_transformation) ||
            cost_partitionings.empty()) {
            cost_partitionings.push_back(cp_factory->generate(
                fts.get_labels(), compute_abstractions_over_fts(fts), verbosity));
            log_progress(timer, "after handling final snapshot");
        }
    }

    if (cost_partitionings.size() == 1) {
        cost_partitionings.back()->print_statistics();
    }

    const bool final = true;
    report_peak_memory_delta(final);
    cout << "Merge-and-shrink algorithm runtime: " << timer << endl;
    cout << endl;
    return cost_partitionings;
}

void add_cp_merge_and_shrink_algorithm_options_to_parser(OptionParser &parser) {
    add_merge_and_shrink_algorithm_options_to_parser(parser);

    // Cost partitioning options
    parser.add_option<shared_ptr<CostPartitioningFactory>>(
        "cost_partitioning",
        "A method for computing cost partitionings over intermediate "
        "'snapshots' of the factored transition system.");
    parser.add_option<bool>(
        "compute_atomic_snapshot",
        "Include a snapshot over the atomic FTS.",
        "false");
    parser.add_option<bool>(
        "compute_final_snapshot",
        "Include a snapshot over the final FTS if this has not already been(attention: "
        "depending on main_loop_target_num_snapshots, this might already have "
        "been computed).",
        "false");
    parser.add_option<int>(
        "main_loop_target_num_snapshots",
        "The aimed number of SCP heuristics to be computed over the main loop.",
        "0",
        Bounds("0", "infinity"));
    parser.add_option<int>(
        "main_loop_snapshot_each_iteration",
        "A number of iterations after which an SCP heuristic is computed over "
        "the current FTS.",
        "0",
        Bounds("0", "infinity"));

    vector<string> snapshot_moment;
    vector<string> snapshot_moment_doc;
    snapshot_moment.push_back("after_label_reduction");
    snapshot_moment_doc.push_back("after 'label reduction before shrinking'");
    snapshot_moment.push_back("after_shrinking");
    snapshot_moment_doc.push_back("after shrinking");
    snapshot_moment.push_back("after_merging");
    snapshot_moment_doc.push_back("after merging");
    snapshot_moment.push_back("after_pruning");
    snapshot_moment_doc.push_back("after pruning, i.e., at end of iteration");
    parser.add_enum_option(
        "snapshot_moment",
        snapshot_moment,
        "the point in one iteration at which a snapshot should be computed",
        "after_label_reduction",
        snapshot_moment_doc);

    parser.add_option<bool>(
        "filter_trivial_factors",
        "If true, do not consider trivial factors for computing CPs. Should "
        "be set to true when computing SCPs.");
}

void handle_cp_merge_and_shrink_algorithm_options(Options &opts) {
    handle_shrink_limit_options_defaults(opts);

    bool compute_atomic_snapshot = opts.get<bool>("compute_atomic_snapshot");
    bool compute_final_snapshot = opts.get<bool>("compute_final_snapshot");
    int main_loop_target_num_snapshots = opts.get<int>("main_loop_target_num_snapshots");
    int main_loop_snapshot_each_iteration =
        opts.get<int>("main_loop_snapshot_each_iteration");
    if (!compute_atomic_snapshot &&
        !compute_final_snapshot &&
        !main_loop_target_num_snapshots &&
        !main_loop_snapshot_each_iteration) {
        cerr << "At least one option for computing SCP merge-and-shrink "
                "heuristics must be enabled! " << endl;
        if (main_loop_target_num_snapshots && main_loop_snapshot_each_iteration) {
            cerr << "Can't set both the number of heuristics and the iteration "
                    "offset in which heuristics are computed."
                 << endl;
        }
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}
}
