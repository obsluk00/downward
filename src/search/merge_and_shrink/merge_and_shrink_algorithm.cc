#include "merge_and_shrink_algorithm.h"

#include "distances.h"
#include "factored_transition_system.h"
#include "fts_factory.h"
#include "label_equivalence_relation.h"
#include "label_reduction.h"
#include "labels.h"
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
#include "../utils/rng.h"
#include "../utils/rng_options.h"
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

MergeAndShrinkAlgorithm::MergeAndShrinkAlgorithm(const Options &opts) :
    merge_strategy_factory(opts.get<shared_ptr<MergeStrategyFactory>>("merge_strategy")),
    shrink_strategy(opts.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
    label_reduction(opts.get<shared_ptr<LabelReduction>>("label_reduction", nullptr)),
    max_states(opts.get<int>("max_states")),
    max_states_before_merge(opts.get<int>("max_states_before_merge")),
    shrink_threshold_before_merge(opts.get<int>("threshold_before_merge")),
    prune_unreachable_states(opts.get<bool>("prune_unreachable_states")),
    prune_irrelevant_states(opts.get<bool>("prune_irrelevant_states")),
    verbosity(static_cast<Verbosity>(opts.get_enum("verbosity"))),
    main_loop_max_time(opts.get<double>("main_loop_max_time")),
    rng(utils::parse_rng_from_options(opts)),
    factor_order(static_cast<FactorOrder>(opts.get_enum("factor_order"))),
    scp_over_atomic_fts(opts.get<bool>("scp_over_atomic_fts")),
    scp_over_final_fts(opts.get<bool>("scp_over_final_fts")),
    main_loop_num_scp_heuristics(opts.get<int>("main_loop_num_scp_heuristics")),
    main_loop_iteration_offset_for_computing_scp_heuristics(
        opts.get<int>("main_loop_iteration_offset_for_computing_scp_heuristics")),
    starting_peak_memory(0) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
    assert(shrink_threshold_before_merge <= max_states_before_merge);
}

void MergeAndShrinkAlgorithm::report_peak_memory_delta(bool final) const {
    if (final)
        cout << "Final";
    else
        cout << "Current";
    cout << " peak memory increase of merge-and-shrink algorithm: "
         << utils::get_peak_memory_in_kb() - starting_peak_memory << " KB"
         << endl;
}

void MergeAndShrinkAlgorithm::dump_options() const {
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

    cout << "Verbosity: ";
    switch (verbosity) {
    case Verbosity::SILENT:
        cout << "silent";
        break;
    case Verbosity::NORMAL:
        cout << "normal";
        break;
    case Verbosity::VERBOSE:
        cout << "verbose";
        break;
    case Verbosity::DEBUG:
        cout << "debug";
        break;
    }
    cout << endl;
}

void MergeAndShrinkAlgorithm::warn_on_unusual_options() const {
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

bool MergeAndShrinkAlgorithm::ran_out_of_time(
    const utils::CountdownTimer &timer) const {
    if (timer.is_expired()) {
        if (verbosity >= Verbosity::NORMAL) {
            cout << "Ran out of time, stopping computation." << endl;
            cout << endl;
        }
        return true;
    }
    return false;
}

class NextSCPHeuristic {
private:
    const double max_time;
    const int max_iterations;
    const int aimed_num_scp_heuristics;
    const double iteration_offset;
    const Verbosity verbosity;

    int last_iteration_computed;
    double next_time_to_compute_heuristic;
    int next_iteration_to_compute_heuristic;

    void set_next_time_to_compute_heuristic(int num_computed_scp_heuristics, double current_time) {
        int num_remaining_scp_heuristics = aimed_num_scp_heuristics - num_computed_scp_heuristics;
        if (!num_remaining_scp_heuristics) {
            next_time_to_compute_heuristic = max_time + 1.0;
            return;
        }
        double remaining_time = max_time - current_time;
        if (remaining_time <= 0.0) {
            next_time_to_compute_heuristic = current_time;
            return;
        }
        double time_offset = remaining_time / static_cast<double>(num_remaining_scp_heuristics);
        next_time_to_compute_heuristic = current_time + time_offset;
    }

    void set_next_iteration_to_compute_heuristic(int num_computed_scp_heuristics, int current_iteration) {
        if (aimed_num_scp_heuristics) {
            int num_remaining_scp_heuristics = aimed_num_scp_heuristics - num_computed_scp_heuristics;
            if (!num_remaining_scp_heuristics) {
                next_iteration_to_compute_heuristic = max_iterations + 1;
                return;
            }
            int num_remaining_iterations = max_iterations - current_iteration;
            if (!num_remaining_iterations || num_remaining_scp_heuristics >= num_remaining_iterations) {
                next_iteration_to_compute_heuristic = current_iteration;
                return;
            }
            double iteration_offset = num_remaining_iterations / static_cast<double>(num_remaining_scp_heuristics);
            assert(iteration_offset >= 1.0);
            next_iteration_to_compute_heuristic = current_iteration + static_cast<int>(iteration_offset);
        } else {
            next_iteration_to_compute_heuristic = current_iteration + iteration_offset;
            if (current_iteration == 0) {
                // To balance off the fact that we start counting at 0.
                next_iteration_to_compute_heuristic -= 1;
            }
        }
    }
public:
    NextSCPHeuristic(
        double max_time,
        int max_iterations,
        int aimed_num_scp_heuristics,
        int iteration_offset,
        Verbosity verbosity)
        : max_time(max_time),
          max_iterations(max_iterations),
          aimed_num_scp_heuristics(aimed_num_scp_heuristics),
          iteration_offset(iteration_offset),
          verbosity(verbosity) {
        if (aimed_num_scp_heuristics || iteration_offset) {
            assert(!aimed_num_scp_heuristics || !iteration_offset);
            set_next_time_to_compute_heuristic(0, 0);
            set_next_iteration_to_compute_heuristic(0, 0);
            if (verbosity == Verbosity::DEBUG) {
                cout << "SCP: next time: " << next_time_to_compute_heuristic
                     << ", next iteration: " << next_iteration_to_compute_heuristic
                     << endl;
            }
        }
    }

    bool compute_next_heuristic(double current_time, int current_iteration, int num_computed_scp_heuristics) {
        if (!aimed_num_scp_heuristics && !iteration_offset) {
            return false;
        }
        if (verbosity == Verbosity::DEBUG) {
            cout << "SCP: compute next heuristic? current time: " << current_time
                 << ", current iteration: " << current_iteration
                 << ", num existing heuristics: " << num_computed_scp_heuristics
                 << endl;
        }
        bool compute = false;
        if (current_time >= next_time_to_compute_heuristic ||
            current_iteration >= next_iteration_to_compute_heuristic) {
            compute = true;
        }
        if (compute) {
            set_next_time_to_compute_heuristic(num_computed_scp_heuristics, current_time);
            set_next_iteration_to_compute_heuristic(num_computed_scp_heuristics, current_iteration);
            if (verbosity == Verbosity::DEBUG) {
                cout << "SCP: yes" << endl;
                cout << "SCP: next time: " << next_time_to_compute_heuristic
                     << ", next iteration: " << next_iteration_to_compute_heuristic
                     << endl;
            }
        }
        return compute;
    }
};

void MergeAndShrinkAlgorithm::main_loop(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy,
    vector<SCPMSHeuristic> *scp_ms_heuristics) {
    utils::CountdownTimer timer(main_loop_max_time);
    if (verbosity >= Verbosity::NORMAL) {
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

    if (label_reduction) {
        label_reduction->initialize(task_proxy);
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
    NextSCPHeuristic next_scp_heuristic(
        main_loop_max_time,
        fts.get_num_active_entries() * 2 - 1,
        main_loop_num_scp_heuristics,
        main_loop_iteration_offset_for_computing_scp_heuristics,
        verbosity);
    while (fts.get_num_active_entries() > 1) {
        // Choose next transition systems to merge
        pair<int, int> merge_indices = merge_strategy->get_next();
        if (ran_out_of_time(timer)) {
            break;
        }
        int merge_index1 = merge_indices.first;
        int merge_index2 = merge_indices.second;
        assert(merge_index1 != merge_index2);
        if (verbosity >= Verbosity::NORMAL) {
            cout << "Next pair of indices: ("
                 << merge_index1 << ", " << merge_index2 << ")" << endl;
            if (verbosity >= Verbosity::VERBOSE) {
                fts.statistics(merge_index1);
                fts.statistics(merge_index2);
            }
            log_main_loop_progress("after computation of next merge");
        }

        // Label reduction (before shrinking)
        if (label_reduction && label_reduction->reduce_before_shrinking()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, verbosity);
            if (verbosity >= Verbosity::NORMAL && reduced) {
                log_main_loop_progress("after label reduction");
            }
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
        if (verbosity >= Verbosity::NORMAL && shrunk) {
            log_main_loop_progress("after shrinking");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Label reduction (before merging)
        if (label_reduction && label_reduction->reduce_before_merging()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, verbosity);
            if (verbosity >= Verbosity::NORMAL && reduced) {
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

        if (verbosity >= Verbosity::NORMAL) {
            if (verbosity >= Verbosity::VERBOSE) {
                fts.statistics(merged_index);
            }
            log_main_loop_progress("after merging");
        }

        // We do not check for num transitions here but only after pruning
        // to allow recovering a too large product.
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
            if (verbosity >= Verbosity::NORMAL && pruned) {
                if (verbosity >= Verbosity::VERBOSE) {
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
            if (verbosity >= Verbosity::NORMAL) {
                cout << "Abstract problem is unsolvable, stopping "
                    "computation. " << endl << endl;
            }
            break;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (scp_ms_heuristics &&
            next_scp_heuristic.compute_next_heuristic(
                timer.get_elapsed_time(), iteration_counter, scp_ms_heuristics->size())) {
            scp_ms_heuristics->push_back(compute_scp_ms_heuristic_over_fts(fts));
            log_main_loop_progress("after computing SCP M&S heuristics");

            if (ran_out_of_time(timer)) {
                break;
            }
        }

        // End-of-iteration output.
        if (verbosity >= Verbosity::VERBOSE) {
            report_peak_memory_delta();
        }
        if (verbosity >= Verbosity::NORMAL) {
            cout << endl;
        }

        ++iteration_counter;
    }

    cout << "End of merge-and-shrink algorithm, statistics:" << endl;
    cout << "Main loop runtime: " << timer.get_elapsed_time() << endl;
    cout << "Maximum intermediate abstraction size: "
         << maximum_intermediate_size << endl;
    shrink_strategy = nullptr;
    label_reduction = nullptr;
}

SCPMSHeuristic MergeAndShrinkAlgorithm::compute_scp_ms_heuristic_over_fts(
    const FactoredTransitionSystem &fts) const {
    if (verbosity >= Verbosity::DEBUG) {
        cout << "Computing SCP M&S heuristic over current FTS..." << endl;
    }

    // Compute original label costs.
    const Labels &labels = fts.get_labels();
    int num_labels = labels.get_size();
    vector<int> remaining_label_costs(num_labels, -1);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels.is_current_label(label_no)) {
            remaining_label_costs[label_no] = labels.get_label_cost(label_no);
        }
    }

    vector<int> active_factor_indices;
    active_factor_indices.reserve(fts.get_num_active_entries());
    for (int index : fts) {
        active_factor_indices.push_back(index);
    }
    if (factor_order == FactorOrder::RANDOM) {
        rng->shuffle(active_factor_indices);
    }

    SCPMSHeuristic scp_ms_heuristic;
    bool dump_if_empty_transitions = true;
    bool dump_if_infinite_transitions = true;
    for (size_t i = 0; i < active_factor_indices.size(); ++i) {
        int index = active_factor_indices[i];
        if (verbosity >= Verbosity::DEBUG) {
            cout << "Considering factor at index " << index << endl;
        }
        const TransitionSystem &ts = fts.get_transition_system(index);
        bool all_goal_states = true;
        for (int state = 0; state < ts.get_size(); ++state) {
            if (!ts.is_goal_state(state)) {
                all_goal_states = false;
                break;
            }
        }
        if (all_goal_states) {
            if (verbosity >= Verbosity::DEBUG) {
                cout << "Factor consists of goal states only, skipping." << endl;
            }
            continue;
        }

//        const Distances &distances = fts.get_distances(index);
//        cout << "Distances under full costs: " << distances.get_goal_distances() << endl;
        if (verbosity >= Verbosity::DEBUG) {
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
        vector<int> goal_distances = compute_goal_distances(
            ts, remaining_label_costs, verbosity);
//        cout << "Distances under remaining costs: " << goal_distances << endl;
        const MergeAndShrinkRepresentation *mas_representation = fts.get_mas_representation_raw_ptr(index);
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representation_raw_ptrs.push_back(mas_representation);
        if (i == active_factor_indices.size() - 1) {
            break;
        }

        // Compute saturated cost of all labels.
        vector<int> saturated_label_costs(remaining_label_costs.size(), -1);
        for (const GroupAndTransitions &gat : ts) {
            const LabelGroup &label_group = gat.label_group;
            const vector<Transition> &transitions = gat.transitions;
            int group_saturated_cost = MINUSINF;
            if (verbosity >= Verbosity::VERBOSE && dump_if_empty_transitions && transitions.empty()) {
                dump_if_empty_transitions = false;
                cout << "found dead label group" << endl;
            } else {
                for (const Transition &transition : transitions) {
                    int src = transition.src;
                    int target = transition.target;
                    int h_src = goal_distances[src];
                    int h_target = goal_distances[target];
                    if (h_target != INF) {
                        int diff = h_src - h_target;
                        group_saturated_cost = max(group_saturated_cost, diff);
                    }
                }
                if (verbosity >= Verbosity::VERBOSE && dump_if_infinite_transitions && group_saturated_cost == MINUSINF) {
                    dump_if_infinite_transitions = false;
                    cout << "label group does not lead to any state with finite heuristic value" << endl;
                }
            }
            for (int label_no : label_group) {
                saturated_label_costs[label_no] = group_saturated_cost;
            }
        }
        if (verbosity >= Verbosity::DEBUG) {
            cout << "Saturated label costs: " << saturated_label_costs << endl;
        }

        // Update remaining label costs.
        for (size_t label_no = 0; label_no < remaining_label_costs.size(); ++label_no) {
            if (remaining_label_costs[label_no] == -1) { // skip reduced labels
                assert(saturated_label_costs[label_no] == -1);
            } else {
                if (saturated_label_costs[label_no] == MINUSINF) {
                    remaining_label_costs[label_no] = INF;
                } else if (remaining_label_costs[label_no] != INF) { // inf remains inf
                    remaining_label_costs[label_no] =
                        remaining_label_costs[label_no] - saturated_label_costs[label_no];
                    assert(remaining_label_costs[label_no] >= 0);
                }
            }
        }
    }

    if (verbosity >= Verbosity::DEBUG) {
        cout << "Done computing SCP M&S heuristic over current FTS." << endl;
    }

    return scp_ms_heuristic;
}

// TODO: reduce code duplication with build_factored_transition_system
SCPMSHeuristics MergeAndShrinkAlgorithm::compute_scp_ms_heuristics(
    const TaskProxy &task_proxy) {
    if (starting_peak_memory) {
        cerr << "Calling compute_scp_ms_heuristics twice is not "
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
    if (verbosity >= Verbosity::NORMAL) {
        log_progress(timer, "after computation of atomic factors");
    }

    // Collect SCP M&S heuristics over the computation of the algorithm.
    SCPMSHeuristics scp_ms_heuristics;

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
            unsolvable = true;
            cout << "Atomic FTS is unsolvable, stopping computation." << endl;
            cout << fts.get_transition_system(index).tag()
                 << "use this unsolvable factor as only heuristic."
                 << endl;

            SCPMSHeuristic scp_ms_heuristic;
            scp_ms_heuristic.goal_distances.reserve(1);
            scp_ms_heuristic.mas_representation_raw_ptrs.reserve(1);
            scp_ms_heuristic.goal_distances.push_back(fts.get_distances(index).get_goal_distances());
            scp_ms_heuristic.mas_representation_raw_ptrs.push_back(fts.get_mas_representation_raw_ptr(index));

            scp_ms_heuristics.scp_ms_heuristics.reserve(1);
            scp_ms_heuristics.scp_ms_heuristics.push_back(move(scp_ms_heuristic));

            auto factor = fts.extract_factor(index);
            scp_ms_heuristics.mas_representations.reserve(1);
            scp_ms_heuristics.mas_representations.push_back(move(factor.first));
            break;
        }
    }
    if (verbosity >= Verbosity::NORMAL && pruned) {
        log_progress(timer, "after pruning atomic factors");
    }

    if (scp_over_atomic_fts && !unsolvable) {
        scp_ms_heuristics.scp_ms_heuristics.push_back(compute_scp_ms_heuristic_over_fts(fts));
        if (verbosity >= Verbosity::NORMAL) {
            log_progress(timer, "after computing SCP M&S heuristics over the atomic FTS");
        }
    }

    if (verbosity >= Verbosity::NORMAL) {
        cout << endl;
    }

    if (!unsolvable) {
        if (main_loop_max_time > 0) {
            main_loop(fts, task_proxy, &scp_ms_heuristics.scp_ms_heuristics);
        }

        if (scp_over_final_fts) {
            scp_ms_heuristics.scp_ms_heuristics.push_back(compute_scp_ms_heuristic_over_fts(fts));
        }

        // Extract the final merge-and-shrink representations from the FTS.
        scp_ms_heuristics.mas_representations.reserve(fts.get_num_active_entries());
        for (int index : fts) {
            scp_ms_heuristics.mas_representations.push_back(fts.extract_factor(index).first);
        }
    }

    const bool final = true;
    report_peak_memory_delta(final);
    cout << "Merge-and-shrink algorithm runtime: " << timer << endl;
    cout << endl;
    return scp_ms_heuristics;
}

FactoredTransitionSystem MergeAndShrinkAlgorithm::build_factored_transition_system(
    const TaskProxy &task_proxy) {
    if (starting_peak_memory) {
        cerr << "Calling build_factored_transition_system twice is not "
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
    if (verbosity >= Verbosity::NORMAL) {
        log_progress(timer, "after computation of atomic factors");
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
            unsolvable = true;
            break;
        }
    }
    if (verbosity >= Verbosity::NORMAL && pruned) {
        log_progress(timer, "after pruning atomic factors");
        cout << endl;
    }

    if (unsolvable) {
        cout << "Atomic FTS is unsolvable, stopping computation." << endl;
    } else if (main_loop_max_time > 0) {
        main_loop(fts, task_proxy);
    }
    const bool final = true;
    report_peak_memory_delta(final);
    cout << "Merge-and-shrink algorithm runtime: " << timer << endl;
    cout << endl;
    return fts;
}

void add_merge_and_shrink_algorithm_options_to_parser(OptionParser &parser) {
    // Merge strategy option.
    parser.add_option<shared_ptr<MergeStrategyFactory>>(
        "merge_strategy",
        "See detailed documentation for merge strategies. "
        "We currently recommend SCC-DFP, which can be achieved using "
        "{{{merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector="
        "score_based_filtering(scoring_functions=[goal_relevance,dfp,total_order"
        "]))}}}");

    // Shrink strategy option.
    parser.add_option<shared_ptr<ShrinkStrategy>>(
        "shrink_strategy",
        "See detailed documentation for shrink strategies. "
        "We currently recommend non-greedy shrink_bisimulation, which can be "
        "achieved using {{{shrink_strategy=shrink_bisimulation(greedy=false)}}}");

    // Label reduction option.
    parser.add_option<shared_ptr<LabelReduction>>(
        "label_reduction",
        "See detailed documentation for labels. There is currently only "
        "one 'option' to use label_reduction, which is {{{label_reduction=exact}}} "
        "Also note the interaction with shrink strategies.",
        OptionParser::NONE);

    // Pruning options.
    parser.add_option<bool>(
        "prune_unreachable_states",
        "If true, prune abstract states unreachable from the initial state.",
        "true");
    parser.add_option<bool>(
        "prune_irrelevant_states",
        "If true, prune abstract states from which no goal state can be "
        "reached.",
        "true");

    add_transition_system_size_limit_options_to_parser(parser);

    vector<string> verbosity_levels;
    vector<string> verbosity_level_docs;
    verbosity_levels.push_back("silent");
    verbosity_level_docs.push_back(
        "silent: no output during construction, only starting and final "
        "statistics");
    verbosity_levels.push_back("normal");
    verbosity_level_docs.push_back(
        "normal: basic output during construction, starting and final "
        "statistics");
    verbosity_levels.push_back("verbose");
    verbosity_level_docs.push_back(
        "verbose: full output during construction, starting and final "
        "statistics");
    verbosity_levels.push_back("debug");
    verbosity_level_docs.push_back(
        "debug: like verbose with additional debug output, not suited for "
        "running experiments");
    parser.add_enum_option(
        "verbosity",
        verbosity_levels,
        "Option to specify the level of verbosity.",
        "verbose",
        verbosity_level_docs);

    parser.add_option<double>(
        "main_loop_max_time",
        "A limit in seconds on the runtime of the main loop of the algorithm. "
        "If the limit is exceeded, the algorithm terminates, potentially "
        "returning a factored transition system with several factors. Also "
        "note that the time limit is only checked between transformations "
        "of the main loop, but not during, so it can be exceeded if a "
        "transformation is runtime-intense.",
        "infinity",
        Bounds("0.0", "infinity"));

    utils::add_rng_options(parser);

    vector<string> factor_order;
    vector<string> factor_order_docs;
    factor_order.push_back("given");
    factor_order_docs.push_back(
        "given: the order of factors as in the FTS");
    factor_order.push_back("random");
    factor_order_docs.push_back(
        "random: random order of factors");
    parser.add_enum_option(
        "factor_order",
        factor_order,
        "Option to specify the order in which factors of the FTS are "
        "considered for computing the SCP.",
        "random",
        factor_order_docs);

    parser.add_option<bool>(
        "scp_over_atomic_fts",
        "Include an SCP heuristic computed over the atomic FTS.",
        "false");
    parser.add_option<bool>(
        "scp_over_final_fts",
        "Include an SCP heuristic computed over the final FTS (attention: "
        "depending on main_loop_num_scp_heuristics, this might already have "
        "been computed).",
        "false");
    parser.add_option<int>(
        "main_loop_num_scp_heuristics",
        "The aimed number of SCP heuristics to be computed over the main loop.",
        "0",
        Bounds("0", "infinity"));
    parser.add_option<int>(
        "main_loop_iteration_offset_for_computing_scp_heuristics",
        "A number of iterations after which an SCP heuristic is computed over "
        "the current FTS.",
        "0",
        Bounds("0", "infinity"));
}

void add_transition_system_size_limit_options_to_parser(OptionParser &parser) {
    parser.add_option<int>(
        "max_states",
        "maximum transition system size allowed at any time point.",
        "-1",
        Bounds("-1", "infinity"));
    parser.add_option<int>(
        "max_states_before_merge",
        "maximum transition system size allowed for two transition systems "
        "before being merged to form the synchronized product.",
        "-1",
        Bounds("-1", "infinity"));
    parser.add_option<int>(
        "threshold_before_merge",
        "If a transition system, before being merged, surpasses this soft "
        "transition system size limit, the shrink strategy is called to "
        "possibly shrink the transition system.",
        "-1",
        Bounds("-1", "infinity"));
}

void handle_shrink_limit_options_defaults(Options &opts) {
    int max_states = opts.get<int>("max_states");
    int max_states_before_merge = opts.get<int>("max_states_before_merge");
    int threshold = opts.get<int>("threshold_before_merge");

    // If none of the two state limits has been set: set default limit.
    if (max_states == -1 && max_states_before_merge == -1) {
        max_states = 50000;
    }

    // If exactly one of the max_states options has been set, set the other
    // so that it imposes no further limits.
    if (max_states_before_merge == -1) {
        max_states_before_merge = max_states;
    } else if (max_states == -1) {
        int n = max_states_before_merge;
        if (utils::is_product_within_limit(n, n, INF)) {
            max_states = n * n;
        } else {
            max_states = INF;
        }
    }

    if (max_states_before_merge > max_states) {
        cout << "warning: max_states_before_merge exceeds max_states, "
             << "correcting." << endl;
        max_states_before_merge = max_states;
    }

    if (max_states < 1) {
        cerr << "error: transition system size must be at least 1" << endl;
        utils::exit_with(ExitCode::SEARCH_INPUT_ERROR);
    }

    if (max_states_before_merge < 1) {
        cerr << "error: transition system size before merge must be at least 1"
             << endl;
        utils::exit_with(ExitCode::SEARCH_INPUT_ERROR);
    }

    if (threshold == -1) {
        threshold = max_states;
    }
    if (threshold < 1) {
        cerr << "error: threshold must be at least 1" << endl;
        utils::exit_with(ExitCode::SEARCH_INPUT_ERROR);
    }
    if (threshold > max_states) {
        cout << "warning: threshold exceeds max_states, correcting" << endl;
        threshold = max_states;
    }

    opts.set<int>("max_states", max_states);
    opts.set<int>("max_states_before_merge", max_states_before_merge);
    opts.set<int>("threshold_before_merge", threshold);
}
}
