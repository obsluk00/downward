#include "cp_mas_offline.h"

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
CPMASOffline::CPMASOffline(const Options &opts) :
    CPMAS(opts) {
}

vector<unique_ptr<Abstraction>> CPMASOffline::compute_abstractions_over_fts_single_cp(
    const FactoredTransitionSystem &fts,
    const set<int> &indices,
    const vector<int> &original_to_current_labels) const {
    assert(!indices.empty());
    vector<int> considered_factors;
    for (int index : indices) {
        if (!filter_trivial_factors || !fts.is_factor_trivial(index)) {
            considered_factors.push_back(index);
        }
    }
    // We allow that all to-be-considered factors be trivial.
    if (considered_factors.empty() && verbosity >= utils::Verbosity::DEBUG) {
        cout << "All factors modified since last transformation are trivial; "
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
            transition_system, move(mas_representation), index, original_to_current_labels));
    }
    return abstractions;
}

bool CPMASOffline::main_loop_single_cp(
    FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy,
    vector<unique_ptr<Abstraction>> &abstractions,
    set<int> &factors_modified_since_last_snapshot,
    vector<int> &original_to_current_labels) {
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
            bool reduced = label_reduction->reduce(
                merge_indices, fts, verbosity, &original_to_current_labels);
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
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            if (!factors_modified_since_last_snapshot.empty()) {
                vector<unique_ptr<Abstraction>> new_abstractions =
                    compute_abstractions_over_fts_single_cp(fts, factors_modified_since_last_snapshot, original_to_current_labels);
                abstractions.insert(
                    abstractions.end(),
                    make_move_iterator(new_abstractions.begin()),
                    make_move_iterator(new_abstractions.end()));
                factors_modified_since_last_snapshot.clear();
                computed_snapshot_after_last_transformation = true;
                if (verbosity >= utils::Verbosity::NORMAL) {
                    log_main_loop_progress("after handling main loop snapshot");
                }
                if (verbosity >= utils::Verbosity::DEBUG) {
                    cout << "Number of abstractions: " << abstractions.size() << endl;
                }
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
                factors_modified_since_last_snapshot.insert(merge_index1);
            }
            if (shrunk.second) {
                factors_modified_since_last_snapshot.insert(merge_index2);
            }
            computed_snapshot_after_last_transformation = false;
        }
        if (verbosity >= utils::Verbosity::NORMAL && (shrunk.first || shrunk.second)) {
            log_main_loop_progress("after shrinking");
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_SHRINKING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            if (!factors_modified_since_last_snapshot.empty()) {
                vector<unique_ptr<Abstraction>> new_abstractions =
                    compute_abstractions_over_fts_single_cp(fts, factors_modified_since_last_snapshot, original_to_current_labels);
                abstractions.insert(
                    abstractions.end(),
                    make_move_iterator(new_abstractions.begin()),
                    make_move_iterator(new_abstractions.end()));
                factors_modified_since_last_snapshot.clear();
                computed_snapshot_after_last_transformation = true;
                if (verbosity >= utils::Verbosity::NORMAL) {
                    log_main_loop_progress("after handling main loop snapshot");
                }
                if (verbosity >= utils::Verbosity::DEBUG) {
                    cout << "Number of abstractions: " << abstractions.size() << endl;
                }
            }
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        // Label reduction (before merging)
        if (label_reduction && label_reduction->reduce_before_merging()) {
            bool reduced = label_reduction->reduce(
                merge_indices, fts, verbosity, &original_to_current_labels);
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

        factors_modified_since_last_snapshot.erase(merge_index1);
        factors_modified_since_last_snapshot.erase(merge_index2);
        factors_modified_since_last_snapshot.insert(merged_index);
        computed_snapshot_after_last_transformation = false;
        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_MERGING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            if (!factors_modified_since_last_snapshot.empty()) {
                vector<unique_ptr<Abstraction>> new_abstractions =
                    compute_abstractions_over_fts_single_cp(fts, factors_modified_since_last_snapshot, original_to_current_labels);
                abstractions.insert(
                    abstractions.end(),
                    make_move_iterator(new_abstractions.begin()),
                    make_move_iterator(new_abstractions.end()));
                factors_modified_since_last_snapshot.clear();
                computed_snapshot_after_last_transformation = true;
                if (verbosity >= utils::Verbosity::NORMAL) {
                    log_main_loop_progress("after handling main loop snapshot");
                }
                if (verbosity >= utils::Verbosity::DEBUG) {
                    cout << "Number of abstractions: " << abstractions.size() << endl;
                }
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
                factors_modified_since_last_snapshot.insert(merged_index);
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
            vector<unique_ptr<Abstraction>>().swap(abstractions);
            vector<unique_ptr<Abstraction>> abstraction = extract_unsolvable_abstraction(fts, merged_index);
            assert(abstraction.size() == 1);
            abstractions.insert(
                abstractions.end(),
                make_move_iterator(abstraction.begin()),
                make_move_iterator(abstraction.end()));
            factors_modified_since_last_snapshot.clear();
            computed_snapshot_after_last_transformation = true;
            break;
        }

        if (ran_out_of_time(timer)) {
            break;
        }

        if (snapshot_moment == SnapshotMoment::AFTER_PRUNING &&
            next_snapshot &&
            next_snapshot->compute_next_snapshot(timer.get_elapsed_time(), iteration_counter)) {
            if (!factors_modified_since_last_snapshot.empty()) {
                vector<unique_ptr<Abstraction>> new_abstractions =
                    compute_abstractions_over_fts_single_cp(fts, factors_modified_since_last_snapshot, original_to_current_labels);
                abstractions.insert(
                    abstractions.end(),
                    make_move_iterator(new_abstractions.begin()),
                    make_move_iterator(new_abstractions.end()));
                factors_modified_since_last_snapshot.clear();
                computed_snapshot_after_last_transformation = true;
                if (verbosity >= utils::Verbosity::NORMAL) {
                    log_main_loop_progress("after handling main loop snapshot");
                }
                if (verbosity >= utils::Verbosity::DEBUG) {
                    cout << "Number of abstractions: " << abstractions.size() << endl;
                }
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

unique_ptr<CostPartitioning> CPMASOffline::compute_single_ms_cp(
    const TaskProxy &task_proxy, CostPartitioningFactory &cp_factory) {
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

    vector<unique_ptr<Abstraction>> abstractions;

    vector<int> label_costs = compute_label_costs(fts.get_labels());

    // Global label mapping.
    vector<int> original_to_current_labels(label_costs.size());
    iota(original_to_current_labels.begin(), original_to_current_labels.end(), 0);

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
            vector<unique_ptr<Abstraction>> abstraction = extract_unsolvable_abstraction(fts, index);
            assert(abstraction.size() == 1);
            abstractions.insert(
                abstractions.end(),
                make_move_iterator(abstraction.begin()),
                make_move_iterator(abstraction.end()));
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

        bool computed_snapshot_after_last_transformation = false;
        if (label_reduction && atomic_label_reduction) {
            bool reduced = label_reduction->reduce(
                pair<int, int>(-1, -1), fts, verbosity,
                &original_to_current_labels);
            if (verbosity >= utils::Verbosity::NORMAL && reduced) {
                log_progress(timer, "after label reduction on atomic FTS");
            }
        }

        set<int> factors_modified_since_last_snapshot; // excluding label reduction
        for (int index = 0; index < fts.get_size(); ++index) {
            factors_modified_since_last_snapshot.insert(index);
        }
        if (compute_atomic_snapshot) {
            vector<unique_ptr<Abstraction>> new_abstractions =
                compute_abstractions_over_fts_single_cp(
                    fts, factors_modified_since_last_snapshot, original_to_current_labels);
            abstractions.insert(
                abstractions.end(),
                make_move_iterator(new_abstractions.begin()),
                make_move_iterator(new_abstractions.end()));
            factors_modified_since_last_snapshot.clear();
            computed_snapshot_after_last_transformation = true;
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling atomic snapshot");
            }
            if (verbosity >= utils::Verbosity::DEBUG) {
                cout << "Number of abstractions: " << abstractions.size() << endl;
            }
        }

        if (verbosity >= utils::Verbosity::NORMAL) {
            cout << endl;
        }

        if (main_loop_max_time > 0) {
            computed_snapshot_after_last_transformation = main_loop_single_cp(
                fts, task_proxy, abstractions,
                factors_modified_since_last_snapshot,
                original_to_current_labels);
        }

        if (computed_snapshot_after_last_transformation) {
            assert(!abstractions.empty());
        }

        if ((compute_final_snapshot && !computed_snapshot_after_last_transformation) ||
            abstractions.empty()) {
            if (abstractions.empty()) {
                assert(!factors_modified_since_last_snapshot.empty());
            } else {
                /*
                  Happens if computing an atomic snapshot and not running the
                  main loop. Use all factors in this case.
                */
                if (factors_modified_since_last_snapshot.empty()) {
                    for (int index : fts) {
                        factors_modified_since_last_snapshot.insert(index);
                    }
                }
            }
            vector<unique_ptr<Abstraction>> new_abstractions =
                compute_abstractions_over_fts_single_cp(fts, factors_modified_since_last_snapshot, original_to_current_labels);
            abstractions.insert(
                abstractions.end(),
                make_move_iterator(new_abstractions.begin()),
                make_move_iterator(new_abstractions.end()));
            if (verbosity >= utils::Verbosity::NORMAL) {
                log_progress(timer, "after handling final snapshot");
            }
            if (verbosity >= utils::Verbosity::DEBUG) {
                cout << "Number of abstractions: " << abstractions.size() << endl;
            }
        }
    }

    unique_ptr<CostPartitioning> cost_partitioning = cp_factory.generate(
        move(label_costs),
        move(abstractions),
        verbosity);
    cost_partitioning->print_statistics();

    const bool final = true;
    report_peak_memory_delta(final);
    cout << "Merge-and-shrink algorithm runtime: " << timer << endl;
    cout << endl;
    return cost_partitioning;
}
}
