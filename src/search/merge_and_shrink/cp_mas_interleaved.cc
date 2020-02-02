#include "cp_mas_interleaved.h"

#include "cost_partitioning.h"
#include "distances.h"
#include "factored_transition_system.h"
#include "fts_factory.h"
#include "label_reduction.h"
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
CPMASInterleaved::CPMASInterleaved(const Options &opts) :
    CPMAS(opts) {
}

void CPMASInterleaved::compute_cp_and_print_statistics(
    const FactoredTransitionSystem &fts,
    int iteration) const {
    std::unique_ptr<CostPartitioning> cp = cp_factory->generate(
        compute_label_costs(fts.get_labels()), compute_abstractions_over_fts(fts), verbosity);
    cout << "CP value in iteration " << iteration << ": "
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
    cout << "Max value in iteration " << iteration << ": " << max_h << endl;
}

void CPMASInterleaved::handle_snapshot(
    FactoredTransitionSystem &fts,
    int unsolvable_index) {
    if (unsolvable_index != -1) {
        vector<unique_ptr<Abstraction>> new_abstractions = extract_unsolvable_abstraction(fts, unsolvable_index);
        assert(new_abstractions.size() == 1);
        vector<unique_ptr<CostPartitioning>>().swap(cost_partitionings);
        cost_partitionings.reserve(1);
        cost_partitionings.push_back(
            cp_factory->generate(
                compute_label_costs(fts.get_labels()), move(new_abstractions), verbosity));
    } else {
        cost_partitionings.push_back(cp_factory->generate(
            compute_label_costs(fts.get_labels()), compute_abstractions_over_fts(fts), verbosity));
    }
}

vector<unique_ptr<Abstraction>> CPMASInterleaved::compute_abstractions_over_fts(
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
        abstractions.push_back(utils::make_unique_ptr<Abstraction>(transition_system, move(mas_representation), index));
    }
    return abstractions;
}

bool CPMASInterleaved::main_loop(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy) {
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
    int number_of_applied_transformations = 1;
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
            handle_snapshot(fts);
            computed_snapshot_after_last_transformation = true;
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
            computed_snapshot_after_last_transformation = false;
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
            handle_snapshot(fts);
            computed_snapshot_after_last_transformation = true;
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_main_loop_progress("after handling main loop snapshot");
            }
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
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            handle_snapshot(fts);
            computed_snapshot_after_last_transformation = true;
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
            handle_snapshot(fts, merged_index);
            computed_snapshot_after_last_transformation = true;
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
            handle_snapshot(fts);
            computed_snapshot_after_last_transformation = true;
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

vector<unique_ptr<CostPartitioning>> CPMASInterleaved::compute_cps(
    const TaskProxy &task_proxy) {
    if (starting_peak_memory) {
        cerr << "Using this factory twice is not supported!" << endl;
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

    cp_factory->initialize(task_proxy);

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
            handle_snapshot(fts, index);
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

        bool computed_snapshot_after_last_transformation = false;
        if (label_reduction && atomic_label_reduction) {
            bool reduced = label_reduction->reduce(pair<int, int>(-1, -1), fts, verbosity);
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_progress(timer, "after label reduction on atomic FTS");
            }
        }

        if (compute_atomic_snapshot) {
            handle_snapshot(fts);
            computed_snapshot_after_last_transformation = true;
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling atomic snapshot");
            }
        }

        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << endl;
        }

        if (main_loop_max_time > 0) {
            computed_snapshot_after_last_transformation =
                main_loop(fts, task_proxy);
        }

        if (computed_snapshot_after_last_transformation) {
            assert(!cost_partitionings.empty());
        }

        if ((compute_final_snapshot && !computed_snapshot_after_last_transformation) ||
            cost_partitionings.empty()) {
            handle_snapshot(fts);
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling final snapshot");
            }
        }
    }

    if (cost_partitionings.size() == 1) {
        cost_partitionings.back()->print_statistics();
    }

    const bool final = true;
    report_peak_memory_delta(final);
    cout << "Merge-and-shrink algorithm runtime: " << timer << endl;
    cout << endl;
    return move(cost_partitionings);
}
}
