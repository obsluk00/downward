#include "cp_mas.h"

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

#include "../tasks/root_task.h"
#include "../task_utils/task_properties.h"

#include "../utils/countdown_timer.h"
#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/math.h"
#include "../utils/system.h"
#include "../utils/timer.h"

#include <cassert>
#include <iostream>
#include <numeric>
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
    utils::g_log << "M&S algorithm timer: " << timer << " (" << msg << ")" << endl;
}

CPMAS::CPMAS(const Options &opts) :
    merge_strategy_factory(opts.get<shared_ptr<MergeStrategyFactory>>("merge_strategy")),
    shrink_strategy(opts.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
    label_reduction(opts.get<shared_ptr<LabelReduction>>("label_reduction", nullptr)),
    max_states(opts.get<int>("max_states")),
    max_states_before_merge(opts.get<int>("max_states_before_merge")),
    shrink_threshold_before_merge(opts.get<int>("threshold_before_merge")),
    prune_unreachable_states(opts.get<bool>("prune_unreachable_states")),
    prune_irrelevant_states(opts.get<bool>("prune_irrelevant_states")),
    verbosity(opts.get<utils::Verbosity>("verbosity")),
    main_loop_max_time(opts.get<double>("main_loop_max_time")),
    atomic_label_reduction(opts.get<bool>("atomic_label_reduction")),
    compute_atomic_snapshot(opts.get<bool>("compute_atomic_snapshot")),
    main_loop_target_num_snapshots(opts.get<int>("main_loop_target_num_snapshots")),
    main_loop_snapshot_each_iteration(opts.get<int>("main_loop_snapshot_each_iteration")),
    snapshot_moment(opts.get<SnapshotMoment>("snapshot_moment")),
    filter_trivial_factors(opts.get<bool>("filter_trivial_factors")),
    statistics_only(opts.get<bool>("statistics_only")),
    offline_cps(opts.get<bool>("offline_cps")),
    cp_factory(opts.get<shared_ptr<CostPartitioningFactory>>("cost_partitioning")),
    starting_peak_memory(0) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
    assert(shrink_threshold_before_merge <= max_states_before_merge);
}

void CPMAS::report_peak_memory_delta(bool final) const {
    if (final)
        utils::g_log << "Final";
    else
        utils::g_log << "Current";
    utils::g_log << " peak memory increase of merge-and-shrink algorithm: "
         << utils::get_peak_memory_in_kb() - starting_peak_memory << " KB"
         << endl;
}

void CPMAS::dump_options() const {
    if (verbosity >= utils::Verbosity::NORMAL) {
        if (merge_strategy_factory) { // deleted after merge strategy extraction
            merge_strategy_factory->dump_options();
            utils::g_log << endl;
        }

        utils::g_log << "Options related to size limits and shrinking: " << endl;
        utils::g_log << "Transition system size limit: " << max_states << endl
             << "Transition system size limit right before merge: "
             << max_states_before_merge << endl;
        utils::g_log << "Threshold to trigger shrinking right before merge: "
             << shrink_threshold_before_merge << endl;
        utils::g_log << endl;

        shrink_strategy->dump_options();
        utils::g_log << endl;

        if (label_reduction) {
            label_reduction->dump_options();
        } else {
            utils::g_log << "Label reduction disabled" << endl;
        }
        utils::g_log << endl;

        utils::g_log << "Main loop max time in seconds: " << main_loop_max_time << endl;
        utils::g_log << endl;
    }
}

void CPMAS::warn_on_unusual_options() const {
    string dashes(79, '=');
    if (!label_reduction) {
        utils::g_log << dashes << endl
             << "WARNING! You did not enable label reduction.\nThis may "
            "drastically reduce the performance of merge-and-shrink!"
             << endl << dashes << endl;
    } else if (label_reduction->reduce_before_merging() && label_reduction->reduce_before_shrinking()) {
        utils::g_log << dashes << endl
             << "WARNING! You set label reduction to be applied twice in each merge-and-shrink\n"
            "iteration, both before shrinking and merging. This double computation effort\n"
            "does not pay off for most configurations!"
             << endl << dashes << endl;
    } else {
        if (label_reduction->reduce_before_shrinking() &&
            (shrink_strategy->get_name() == "f-preserving"
             || shrink_strategy->get_name() == "random")) {
            utils::g_log << dashes << endl
                 << "WARNING! Bucket-based shrink strategies such as f-preserving random perform\n"
                "best if used with label reduction before merging, not before shrinking!"
                 << endl << dashes << endl;
        }
        if (label_reduction->reduce_before_merging() &&
            shrink_strategy->get_name() == "bisimulation") {
            utils::g_log << dashes << endl
                 << "WARNING! Shrinking based on bisimulation performs best if used with label\n"
                "reduction before shrinking, not before merging!"
                 << endl << dashes << endl;
        }
    }

    if (!prune_unreachable_states || !prune_irrelevant_states) {
        utils::g_log << dashes << endl
             << "WARNING! Pruning is (partially) turned off!\nThis may "
            "drastically reduce the performance of merge-and-shrink!"
             << endl << dashes << endl;
    }
}

bool CPMAS::ran_out_of_time(
    const utils::CountdownTimer &timer) const {
    if (timer.is_expired()) {
        if (verbosity >= utils::Verbosity::NORMAL) {
            utils::g_log << "Ran out of time, stopping computation." << endl;
            utils::g_log << endl;
        }
        return true;
    }
    return false;
}

void CPMAS::NextSnapshot::compute_next_snapshot_time(double current_time) {
    int num_remaining_snapshots = main_loop_target_num_snapshots - num_main_loop_snapshots;
    // safeguard against having num_remaining_snapshots = 0
    if (num_remaining_snapshots <= 0) {
        next_time_to_compute_snapshot = max_time + 1.0;
        return;
    }
    double remaining_time = max_time - current_time;
    if (remaining_time <= 0.0) {
        next_time_to_compute_snapshot = current_time;
        return;
    }
    double time_offset = remaining_time / static_cast<double>(num_remaining_snapshots);
    next_time_to_compute_snapshot = current_time + time_offset;
}

void CPMAS::NextSnapshot::compute_next_snapshot_iteration(int current_iteration) {
    if (main_loop_target_num_snapshots) {
        int num_remaining_snapshots = main_loop_target_num_snapshots - num_main_loop_snapshots;
        // safeguard against having num_remaining_snapshots = 0
        if (num_remaining_snapshots <= 0) {
            next_iteration_to_compute_snapshot = max_iterations + 1;
            return;
        }
        int num_remaining_iterations = max_iterations - current_iteration;
        if (!num_remaining_iterations || num_remaining_snapshots >= num_remaining_iterations) {
            next_iteration_to_compute_snapshot = current_iteration + 1;
            return;
        }
        double iteration_offset = num_remaining_iterations / static_cast<double>(num_remaining_snapshots);
        assert(iteration_offset >= 1.0);
        next_iteration_to_compute_snapshot = current_iteration + static_cast<int>(iteration_offset);
    } else {
        next_iteration_to_compute_snapshot = current_iteration + main_loop_snapshot_each_iteration;
    }
}

CPMAS::NextSnapshot::NextSnapshot(
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
    if (verbosity >= utils::Verbosity::DEBUG) {
        utils::g_log << "Snapshot collector: next time: " << next_time_to_compute_snapshot
             << ", next iteration: " << next_iteration_to_compute_snapshot
             << endl;
    }
}

bool CPMAS::NextSnapshot::compute_next_snapshot(double current_time, int current_iteration) {
    if (!main_loop_target_num_snapshots && !main_loop_snapshot_each_iteration) {
        return false;
    }
    if (verbosity >= utils::Verbosity::DEBUG) {
        utils::g_log << "Snapshot collector: compute next snapshot? current time: " << current_time
             << ", current iteration: " << current_iteration
             << ", num existing snapshots: " << num_main_loop_snapshots
             << endl;
    }
    bool compute = false;
    if (current_time >= next_time_to_compute_snapshot ||
        current_iteration >= next_iteration_to_compute_snapshot) {
        compute = true;
    }
    if (compute) {
        ++num_main_loop_snapshots; // Assume that we already computed the next snapshot.
        compute_next_snapshot_time(current_time);
        compute_next_snapshot_iteration(current_iteration);
        if (verbosity >= utils::Verbosity::DEBUG) {
            utils::g_log << "Compute snapshot now" << endl;
            utils::g_log << "Next snapshot: next time: " << next_time_to_compute_snapshot
                 << ", next iteration: " << next_iteration_to_compute_snapshot
                 << endl;
        }
    }
    return compute;
}

vector<int> CPMAS::compute_label_costs(
    const Labels &labels) const {
    int num_labels = labels.get_size();
    vector<int> label_costs(num_labels, -1);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels.is_current_label(label_no)) {
            label_costs[label_no] = labels.get_label_cost(label_no);
        }
    }
    return label_costs;
}

vector<unique_ptr<Abstraction>> CPMAS::extract_unsolvable_abstraction(
    FactoredTransitionSystem &fts, int unsolvable_index) const {
    vector<unique_ptr<Abstraction>> abstractions;
    abstractions.reserve(1);
    auto factor = fts.extract_ts_and_representation(unsolvable_index);
    abstractions.push_back(utils::make_unique_ptr<Abstraction>(
        factor.first.release(), move(factor.second)));
    return abstractions;
}

void CPMAS::handle_unsolvable_snapshot(
    FactoredTransitionSystem &fts, int unsolvable_index) {
    vector<unique_ptr<Abstraction>> new_abstractions = extract_unsolvable_abstraction(fts, unsolvable_index);
    assert(new_abstractions.size() == 1);
    if (offline_cps) {
        vector<unique_ptr<Abstraction>>().swap(abstractions);
    } else {
        vector<unique_ptr<CostPartitioning>>().swap(cost_partitionings);
    }
    cost_partitionings.reserve(1);
    cost_partitionings.push_back(
        cp_factory->generate(
            compute_label_costs(fts.get_labels()), move(new_abstractions), verbosity));
}

vector<unique_ptr<Abstraction>> CPMAS::compute_abstractions_for_interleaved_cp(
    const FactoredTransitionSystem &fts) const {
    vector<int> considered_factors;
    considered_factors.reserve(fts.get_num_active_entries());
    for (int index : fts) {
        if (!filter_trivial_factors || !fts.is_factor_trivial(index)) {
            considered_factors.push_back(index);
        }
    }
    assert(!considered_factors.empty());

    vector<unique_ptr<Abstraction>> abstractions;
    abstractions.reserve(considered_factors.size());
    for (int index : considered_factors) {
        assert(fts.is_active(index));
        const TransitionSystem *transition_system = fts.get_transition_system_raw_ptr(index);
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
    return abstractions;
}

bool any(const Bitset &bitset) {
    for (size_t index = 0; index < bitset.size(); ++index) {
        if (bitset.test(index)) {
            return true;
        }
    }
    return false;
}

vector<unique_ptr<Abstraction>> CPMAS::compute_abstractions_for_offline_cp(
    const FactoredTransitionSystem &fts,
    const Bitset &factors_modified_since_last_snapshot,
    const vector<int> &original_to_current_labels) const {
    vector<int> considered_factors;
    for (int index : fts) {
        if (factors_modified_since_last_snapshot.test(index) && (!filter_trivial_factors || !fts.is_factor_trivial(index))) {
            considered_factors.push_back(index);
        }
    }
    // We allow that all to-be-considered factors be trivial.
    if (considered_factors.empty() && verbosity >= utils::Verbosity::DEBUG) {
        utils::g_log << "All factors modified since last transformation are trivial; "
                "no abstraction will be computed" << endl;
    }

    vector<unique_ptr<Abstraction>> abstractions;
    abstractions.reserve(considered_factors.size());
    for (int index : considered_factors) {
        assert(fts.is_active(index));
        TransitionSystem *transition_system = new TransitionSystem(fts.get_transition_system(index));
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
        abstractions.push_back(utils::make_unique_ptr<Abstraction>(
            transition_system, move(mas_representation), original_to_current_labels));
    }
    return abstractions;
}

void CPMAS::handle_snapshot(
    const FactoredTransitionSystem &fts,
    Bitset &factors_modified_since_last_snapshot,
    const unique_ptr<vector<int>> &original_to_current_labels) {
    if (offline_cps) {
        assert(original_to_current_labels);
        vector<unique_ptr<Abstraction>> new_abstractions = compute_abstractions_for_offline_cp(
            fts, factors_modified_since_last_snapshot, *original_to_current_labels);
        abstractions.insert(
            abstractions.end(),
            make_move_iterator(new_abstractions.begin()),
            make_move_iterator(new_abstractions.end()));
        if (verbosity >= utils::Verbosity::DEBUG) {
            utils::g_log << "Number of abstractions: " << abstractions.size() << endl;
        }
    } else if (any(factors_modified_since_last_snapshot)) {
        cost_partitionings.push_back(cp_factory->generate(
            compute_label_costs(fts.get_labels()), compute_abstractions_for_interleaved_cp(fts), verbosity));
    }
    factors_modified_since_last_snapshot.reset();
}

void CPMAS::compute_cp_and_print_statistics(
    const FactoredTransitionSystem &fts,
    int iteration) const {
    std::unique_ptr<CostPartitioning> cp = cp_factory->generate(
        compute_label_costs(fts.get_labels()), compute_abstractions_for_interleaved_cp(fts), verbosity);
    utils::g_log << "CP value in iteration " << iteration << ": "
         << cp->compute_value(
            State(*tasks::g_root_task,
                  tasks::g_root_task->get_initial_state_values()))
         << endl;
    int max_h = 0;
    for (int index : fts) {
        int h = fts.get_distances(index).get_goal_distance(
            fts.get_transition_system(index).get_init_state());
        max_h = max(max_h, h);
    }
    utils::g_log << "Max value in iteration " << iteration << ": " << max_h << endl;
}

bool CPMAS::main_loop(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy,
    Bitset &factors_modified_since_last_snapshot,
    const unique_ptr<vector<int>> &original_to_current_labels) {
    utils::CountdownTimer timer(main_loop_max_time);
    if (verbosity >= utils::Verbosity::NORMAL) {
        utils::g_log << "Starting main loop ";
        if (main_loop_max_time == numeric_limits<double>::infinity()) {
            utils::g_log << "without a time limit." << endl;
        } else {
            utils::g_log << "with a time limit of "
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
            utils::g_log << "M&S algorithm main loop timer: "
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
    int number_of_applied_transformations = 1;
    bool unsolvable = false;
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
            utils::g_log << "Next pair of indices: ("
                 << merge_index1 << ", " << merge_index2 << ")" << endl;
            if (verbosity >= utils::Verbosity::VERBOSE) {
                fts.statistics(merge_index1);
                fts.statistics(merge_index2);
            }
            log_main_loop_progress("after computation of next merge");
        }

        // Label reduction (before shrinking)
        if (label_reduction && label_reduction->reduce_before_shrinking()) {
            bool reduced = label_reduction->reduce(
                merge_indices, fts, verbosity, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_main_loop_progress("after label reduction");
            }
            if (statistics_only && reduced) {
                compute_cp_and_print_statistics(fts, number_of_applied_transformations);
                ++number_of_applied_transformations;
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_LABEL_REDUCTION &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            handle_snapshot(
                fts, factors_modified_since_last_snapshot, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_main_loop_progress("after handling main loop snapshot");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Shrinking
        pair<bool, bool> shrunk = shrink_before_merge_step(
            fts,
            merge_index1,
            merge_index2,
            max_states,
            max_states_before_merge,
            shrink_threshold_before_merge,
            *shrink_strategy,
            verbosity);
        if (shrunk.first || shrunk.second) {
            if (shrunk.first) {
                factors_modified_since_last_snapshot.set(merge_index1);
            }
            if (shrunk.second) {
                factors_modified_since_last_snapshot.set(merge_index2);
            }
        }
        if (verbosity >= utils::Verbosity::NORMAL && (shrunk.first || shrunk.second)) {
            log_main_loop_progress("after shrinking");
        }
        if (statistics_only && (shrunk.first || shrunk.second)) {
            compute_cp_and_print_statistics(fts, number_of_applied_transformations);
            ++number_of_applied_transformations;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_SHRINKING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            handle_snapshot(
                fts, factors_modified_since_last_snapshot, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_main_loop_progress("after handling main loop snapshot");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Label reduction (before merging)
        if (label_reduction && label_reduction->reduce_before_merging()) {
            bool reduced = label_reduction->reduce(
                merge_indices, fts, verbosity, original_to_current_labels);
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

        factors_modified_since_last_snapshot.reset(merge_index1);
        factors_modified_since_last_snapshot.reset(merge_index2);
        factors_modified_since_last_snapshot.set(merged_index);
        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_MERGING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            handle_snapshot(
                fts, factors_modified_since_last_snapshot, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_main_loop_progress("after handling main loop snapshot");
            }
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
                factors_modified_since_last_snapshot.set(merged_index);
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
                utils::g_log << "Abstract problem is unsolvable, stopping "
                    "computation. " << endl << endl;
            }
            handle_unsolvable_snapshot(fts, merged_index);
            factors_modified_since_last_snapshot.reset();
            unsolvable = true;
            break;
        }

        if (statistics_only) {
            compute_cp_and_print_statistics(fts, number_of_applied_transformations);
            ++number_of_applied_transformations;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_PRUNING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            handle_snapshot(
                fts, factors_modified_since_last_snapshot, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_main_loop_progress("after handling main loop snapshot");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // End-of-iteration output.
        if (verbosity >= utils::Verbosity::VERBOSE) {
            report_peak_memory_delta();
        }
        if (verbosity >= utils::Verbosity::NORMAL) {
            utils::g_log << endl;
        }
    }

    utils::g_log << "End of merge-and-shrink algorithm, statistics:" << endl;
    utils::g_log << "Main loop runtime: " << timer.get_elapsed_time() << endl;
    utils::g_log << "Maximum intermediate abstraction size: "
         << maximum_intermediate_size << endl;
    shrink_strategy = nullptr;
    label_reduction = nullptr;
    return unsolvable;
}

vector<unique_ptr<CostPartitioning>> CPMAS::compute_cps(
    const shared_ptr<AbstractTask> &task) {
    if (starting_peak_memory) {
        cerr << "Using this factory twice is not supported!" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    starting_peak_memory = utils::get_peak_memory_in_kb();

    utils::Timer timer;
    utils::g_log << "Running merge-and-shrink algorithm..." << endl;
    TaskProxy task_proxy(*task);
    task_properties::verify_no_axioms(task_proxy);
    dump_options();
    warn_on_unusual_options();
    utils::g_log << endl;

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

    cp_factory->initialize(task);

    // Global label mapping.
    unique_ptr<vector<int>> original_to_current_labels = nullptr;
    if (offline_cps) {
        original_to_current_labels = utils::make_unique_ptr<vector<int>>();
        original_to_current_labels->resize(fts.get_labels().get_size());
        iota(original_to_current_labels->begin(), original_to_current_labels->end(), 0);
    }

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
            utils::g_log << "Atomic FTS is unsolvable, stopping computation." << endl;
            unsolvable = true;
            handle_unsolvable_snapshot(fts, index);
            break;
        }
    }
    if (verbosity >= utils::Verbosity::NORMAL) {
        if (pruned) {
            log_progress(timer, "after pruning atomic factors");
        }
    }

    if (!unsolvable) {
        if (statistics_only) {
            compute_cp_and_print_statistics(fts, 0);
        }

        if (label_reduction) {
            label_reduction->initialize(task_proxy);
        }

        if (label_reduction && atomic_label_reduction) {
            bool reduced = label_reduction->reduce(
                pair<int, int>(-1, -1), fts, verbosity,
                original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_progress(timer, "after label reduction on atomic FTS");
            }
        }

        // Pos at index i is true iff the factor has been transformed since
        // the last recorded snapshot, excluding label reductions.
        Bitset factors_modified_since_last_snapshot(fts.get_size() * 2 - 1);
        for (int index = 0; index < fts.get_size(); ++index) {
            factors_modified_since_last_snapshot.set(index);
        }
        if (compute_atomic_snapshot) {
            handle_snapshot(
                fts, factors_modified_since_last_snapshot, original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling atomic snapshot");
            }
        }

        if (verbosity >= utils::Verbosity::NORMAL) {
            utils::g_log << endl;
        }

        if (main_loop_max_time > 0) {
           unsolvable = main_loop(
                fts, task_proxy,
                factors_modified_since_last_snapshot,
                original_to_current_labels);
        }

        if (!unsolvable) {
            if (!any(factors_modified_since_last_snapshot)) {
                assert((offline_cps && !abstractions.empty()) || (!offline_cps && !cost_partitionings.empty()));
            }

            if (any(factors_modified_since_last_snapshot) ||
                (offline_cps && abstractions.empty()) ||
                (!offline_cps && cost_partitionings.empty())) {
                assert(any(factors_modified_since_last_snapshot));
                handle_snapshot(
                    fts, factors_modified_since_last_snapshot, original_to_current_labels);

                if (verbosity >= utils::Verbosity::NORMAL) {
                    log_progress(timer, "after handling final snapshot");
                }
            }
        }
    }

    if (offline_cps) {
        if (unsolvable) {
            assert(abstractions.empty());
            assert(cost_partitionings.size() == 1);
        } else {
            assert(cost_partitionings.empty());
            // Compute original label costs.
            vector<int> label_costs;
            label_costs.reserve(task_proxy.get_operators().size());
            for (OperatorProxy op : task_proxy.get_operators()) {
                label_costs.push_back(op.get_cost());
            }
            cost_partitionings.reserve(1);
            cost_partitionings.push_back(cp_factory->generate(
                move(label_costs),
                move(abstractions),
                verbosity));
        }
        assert(cost_partitionings.size() == 1);
        utils::g_log << "Offline CPs: number of abstractions: "
             << cost_partitionings.back()->get_number_of_abstractions() << endl;
    } else {
        assert(!cost_partitionings.empty());
        int num_cps = cost_partitionings.size();
        utils::g_log << "Interleaved CPs: number of CPs: "
             << num_cps << endl;
        int summed_num_factors = 0;
        for (const auto &cp : cost_partitionings) {
            summed_num_factors += cp->get_number_of_abstractions();
        }
        double average_num_factors = static_cast<double>(summed_num_factors) /
            static_cast<double>(cost_partitionings.size());
        utils::g_log << "Interleaved CPs: average number of abstractions per CP: "
             << average_num_factors << endl;
    }

    const bool final = true;
    report_peak_memory_delta(final);
    utils::g_log << "Merge-and-shrink algorithm runtime: " << timer << endl;
    utils::g_log << endl;
    return move(cost_partitionings);
}

void add_cp_merge_and_shrink_algorithm_options_to_parser(OptionParser &parser) {
    add_merge_and_shrink_algorithm_options_to_parser(parser);

    // Cost partitioning options
    parser.add_option<bool>(
        "compute_atomic_snapshot",
        "Include a snapshot over the atomic FTS.",
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
    parser.add_enum_option<SnapshotMoment>(
        "snapshot_moment",
        snapshot_moment,
        "the point in one iteration at which a snapshot should be computed",
        "after_label_reduction",
        snapshot_moment_doc);

    parser.add_option<bool>(
        "filter_trivial_factors",
        "If true, do not consider trivial factors for computing CPs. Should "
        "be set to true when computing SCPs.");

    parser.add_option<bool>(
        "statistics_only",
        "If true, compute a CP and the maximum over all factors "
        "after each transformation.",
        "false");

    parser.add_option<bool>(
        "offline_cps",
        "If true, collect all modified abstractions of each snapshot over the"
        "entire M&S algorithm run and then compue one or several CPs over them. "
        "Otherwise, compute a CP for each snapshot during the M&S algorithm. ",
        "true");

    parser.add_option<shared_ptr<CostPartitioningFactory>>(
        "cost_partitioning",
        "A method for computing cost partitionings over intermediate "
        "'snapshots' of the factored transition system.");
}

void handle_cp_merge_and_shrink_algorithm_options(Options &opts) {
    handle_shrink_limit_options_defaults(opts);

    int main_loop_target_num_snapshots = opts.get<int>("main_loop_target_num_snapshots");
    int main_loop_snapshot_each_iteration =
        opts.get<int>("main_loop_snapshot_each_iteration");
    if (main_loop_target_num_snapshots && main_loop_snapshot_each_iteration) {
        cerr << "Can't set both the number of snapshots and the iteration "
                "offset in which snapshots are computed."
             << endl;
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }
}
}
