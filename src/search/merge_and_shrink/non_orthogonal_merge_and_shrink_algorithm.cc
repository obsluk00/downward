#include "non_orthogonal_merge_and_shrink_algorithm.h"

#include "merge_and_shrink_algorithm.h"
#include "distances.h"
#include "factored_transition_system.h"
#include "fts_factory.h"
#include "label_reduction.h"
#include "labels.h"
#include "merge_and_shrink_representation.h"
#include "merge_strategy.h"
#include "merge_strategy_factory.h"
#include "shrink_strategy.h"
#include "transition_system.h"
#include "types.h"
#include "utils.h"

#include "../plugins/plugin.h"
#include "../task_utils/task_properties.h"
#include "../utils/countdown_timer.h"
#include "../utils/markup.h"
#include "../utils/math.h"
#include "../utils/system.h"
#include "../utils/timer.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using plugins::Bounds;
using utils::ExitCode;

namespace merge_and_shrink {
static void log_progress(const utils::Timer &timer, string msg, utils::LogProxy &log) {
    log << "M&S algorithm timer: " << timer << " (" << msg << ")" << endl;
}

NonOrthogonalMergeAndShrinkAlgorithm::NonOrthogonalMergeAndShrinkAlgorithm(const plugins::Options &opts) :
    merge_strategy_factory(opts.get<shared_ptr<MergeStrategyFactory>>("merge_strategy")),
    shrink_strategy(opts.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
    label_reduction(opts.get<shared_ptr<LabelReduction>>("label_reduction", nullptr)),
    max_states(opts.get<int>("max_states")),
    max_states_before_merge(opts.get<int>("max_states_before_merge")),
    shrink_threshold_before_merge(opts.get<int>("threshold_before_merge")),
    prune_unreachable_states(opts.get<bool>("prune_unreachable_states")),
    prune_irrelevant_states(opts.get<bool>("prune_irrelevant_states")),
    tokens(opts.get<int>("tokens")),
    log(utils::get_log_from_options(opts)),
    main_loop_max_time(opts.get<double>("main_loop_max_time")),
    starting_peak_memory(0) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
    assert(shrink_threshold_before_merge <= max_states_before_merge);
}

void NonOrthogonalMergeAndShrinkAlgorithm::report_peak_memory_delta(bool final) const {
    if (final)
        log << "Final";
    else
        log << "Current";
    log << " peak memory increase of merge-and-shrink algorithm: "
        << utils::get_peak_memory_in_kb() - starting_peak_memory << " KB"
        << endl;
}

void NonOrthogonalMergeAndShrinkAlgorithm::dump_options() const {
    if (log.is_at_least_normal()) {
        if (merge_strategy_factory) { // deleted after merge strategy extraction
            merge_strategy_factory->dump_options();
            log << endl;
        }

        log << "Options related to size limits and shrinking: " << endl;
        log << "Transition system size limit: " << max_states << endl
            << "Transition system size limit right before merge: "
            << max_states_before_merge << endl;
        log << "Threshold to trigger shrinking right before merge: "
            << shrink_threshold_before_merge << endl;
        log << endl;

        shrink_strategy->dump_options(log);
        log << endl;

        log << "Pruning unreachable states: "
            << (prune_unreachable_states ? "yes" : "no") << endl;
        log << "Pruning irrelevant states: "
            << (prune_irrelevant_states ? "yes" : "no") << endl;
        log << endl;

        if (label_reduction) {
            label_reduction->dump_options(log);
        } else {
            log << "Label reduction disabled" << endl;
        }
        log << endl;

        log << "Main loop max time in seconds: " << main_loop_max_time << endl;
        log << endl;
    }
}

void NonOrthogonalMergeAndShrinkAlgorithm::warn_on_unusual_options() const {
    string dashes(79, '=');
    if (!label_reduction) {
        if (log.is_warning()) {
            log << dashes << endl
                << "WARNING! You did not enable label reduction. " << endl
                << "This may drastically reduce the performance of merge-and-shrink!"
                << endl << dashes << endl;
        }
    } else if (label_reduction->reduce_before_merging() && label_reduction->reduce_before_shrinking()) {
        if (log.is_warning()) {
            log << dashes << endl
                << "WARNING! You set label reduction to be applied twice in each merge-and-shrink" << endl
                << "iteration, both before shrinking and merging. This double computation effort" << endl
                << "does not pay off for most configurations!"
                << endl << dashes << endl;
        }
    } else {
        if (label_reduction->reduce_before_shrinking() &&
            (shrink_strategy->get_name() == "f-preserving"
             || shrink_strategy->get_name() == "random")) {
            if (log.is_warning()) {
                log << dashes << endl
                    << "WARNING! Bucket-based shrink strategies such as f-preserving random perform" << endl
                    << "best if used with label reduction before merging, not before shrinking!"
                    << endl << dashes << endl;
            }
        }
        if (label_reduction->reduce_before_merging() &&
            shrink_strategy->get_name() == "bisimulation") {
            if (log.is_warning()) {
                log << dashes << endl
                    << "WARNING! Shrinking based on bisimulation performs best if used with label" << endl
                    << "reduction before shrinking, not before merging!"
                    << endl << dashes << endl;
            }
        }
    }

    if (!prune_unreachable_states || !prune_irrelevant_states) {
        if (log.is_warning()) {
            log << dashes << endl
                << "WARNING! Pruning is (partially) turned off!" << endl
                << "This may drastically reduce the performance of merge-and-shrink!"
                << endl << dashes << endl;
        }
    }
}

bool NonOrthogonalMergeAndShrinkAlgorithm::ran_out_of_time(
    const utils::CountdownTimer &timer) const {
    if (timer.is_expired()) {
        if (log.is_at_least_normal()) {
            log << "Ran out of time, stopping computation." << endl;
            log << endl;
        }
        return true;
    }
    return false;
}

void NonOrthogonalMergeAndShrinkAlgorithm::main_loop(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy) {
    utils::CountdownTimer timer(main_loop_max_time);
    if (log.is_at_least_normal()) {
        log << "Starting main loop ";
        if (main_loop_max_time == numeric_limits<double>::infinity()) {
            log << "without a time limit." << endl;
        } else {
            log << "with a time limit of "
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

    auto log_main_loop_progress = [&timer, this](const string &msg) {
            log << "M&S algorithm main loop timer: "
                << timer.get_elapsed_time()
                << " (" << msg << ")" << endl;
        };
    int iteration_counter = 0;

    // TODO: pass as option
    int clone_tokens = tokens;
    // TODO: maybe a better solution to determining adhoc cloning factors
    fts.clone_factor(0);
    fts.remove_factor(0);

    while (fts.get_num_active_entries() > 1) {
        // Choose next transition systems to merge
        pair<int, int> merge_indices = merge_strategy->get_next();
        if (ran_out_of_time(timer)) {
            break;
        }

        int merge_index1 = merge_indices.first;
        int merge_index2 = merge_indices.second;
        // Translate and clone indices if the strategy informs us that the returned ones occur multiple times
        if (merge_index1 < 0) {
            merge_index1 = abs(merge_index1);
            if (clone_tokens > 0) {
                fts.clone_factor(merge_index1);
                clone_tokens -= 1;
            }
        }
        if (merge_index2 < 0) {
            merge_index2 = abs(merge_index2);
            if (clone_tokens > 0) {
                fts.clone_factor(merge_index2);
                clone_tokens -= 1;
            }
        }

        assert(merge_index1 != merge_index2);
        if (log.is_at_least_normal()) {
            log << "Next pair of indices: ("
                << merge_index1 << ", " << merge_index2 << ")" << endl;
            if (log.is_at_least_verbose()) {
                fts.statistics(merge_index1, log);
                fts.statistics(merge_index2, log);
            }
            log_main_loop_progress("after computation of next merge");
        }

        // Label reduction (before shrinking)
        if (label_reduction && label_reduction->reduce_before_shrinking()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, log);
            if (log.is_at_least_normal() && reduced) {
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
            log);
        if (log.is_at_least_normal() && shrunk) {
            log_main_loop_progress("after shrinking");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Label reduction (before merging)
        if (label_reduction && label_reduction->reduce_before_merging()) {
            bool reduced = label_reduction->reduce(merge_indices, fts, log);
            if (log.is_at_least_normal() && reduced) {
                log_main_loop_progress("after label reduction");
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Merging
        int merged_index = fts.merge(merge_index1, merge_index2, log);
        int abs_size = fts.get_transition_system(merged_index).get_size();
        if (abs_size > maximum_intermediate_size) {
            maximum_intermediate_size = abs_size;
        }

        if (log.is_at_least_normal()) {
            if (log.is_at_least_verbose()) {
                fts.statistics(merged_index, log);
            }
            log_main_loop_progress("after merging");
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
                log);
            if (log.is_at_least_normal() && pruned) {
                if (log.is_at_least_verbose()) {
                    fts.statistics(merged_index, log);
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
            if (log.is_at_least_normal()) {
                log << "Abstract problem is unsolvable, stopping "
                    "computation. " << endl << endl;
            }
            break;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // End-of-iteration output.
        if (log.is_at_least_verbose()) {
            report_peak_memory_delta();
        }
        if (log.is_at_least_normal()) {
            log << endl;
        }

        ++iteration_counter;
    }

    // TODO: (mas reps variable counts sum -#var)/#var

    log << "End of merge-and-shrink algorithm, statistics:" << endl;
    log << "Main loop runtime: " << timer.get_elapsed_time() << endl;
    log << "Maximum intermediate abstraction size: "
        << maximum_intermediate_size << endl;
    log << "Times cloned: " << tokens - clone_tokens << endl;
    shrink_strategy = nullptr;
    label_reduction = nullptr;
}

FactoredTransitionSystem NonOrthogonalMergeAndShrinkAlgorithm::build_factored_transition_system(
    const TaskProxy &task_proxy) {
    if (starting_peak_memory) {
        cerr << "Calling build_factored_transition_system twice is not "
             << "supported!" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    starting_peak_memory = utils::get_peak_memory_in_kb();

    utils::Timer timer;
    log << "Running merge-and-shrink algorithm..." << endl;
    task_properties::verify_no_axioms(task_proxy);
    dump_options();
    warn_on_unusual_options();
    log << endl;

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
            log);
    if (log.is_at_least_normal()) {
        log_progress(timer, "after computation of atomic factors", log);
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
                log);
            pruned = pruned || pruned_factor;
        }
        if (!fts.is_factor_solvable(index)) {
            log << "Atomic FTS is unsolvable, stopping computation." << endl;
            unsolvable = true;
            break;
        }
    }
    if (log.is_at_least_normal()) {
        if (pruned) {
            log_progress(timer, "after pruning atomic factors", log);
        }
        log << endl;
    }

    if (!unsolvable && main_loop_max_time > 0) {
        main_loop(fts, task_proxy);
    }
    const bool final = true;
    report_peak_memory_delta(final);
    log << "Merge-and-shrink algorithm runtime: " << timer << endl;
    log << endl;
    return fts;
}
}
