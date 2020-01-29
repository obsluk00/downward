#include "cp_mas.h"

#include "cost_partitioning.h"
#include "factored_transition_system.h"
#include "label_reduction.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "merge_strategy_factory.h"
#include "shrink_strategy.h"
#include "transition_system.h"
#include "types.h"
#include "utils.h"

#include "../options/option_parser.h"
#include "../options/options.h"

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

using namespace std;
using options::Bounds;
using options::OptionParser;
using options::Options;
using utils::ExitCode;

namespace merge_and_shrink {
CPMAS::CPMAS(const Options &opts) :
    merge_strategy_factory(opts.get<shared_ptr<MergeStrategyFactory>>("merge_strategy")),
    shrink_strategy(opts.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
    label_reduction(opts.get<shared_ptr<LabelReduction>>("label_reduction", nullptr)),
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
    statistics_only(opts.get<bool>("statistics_only")),
    starting_peak_memory(0) {
    assert(max_states_before_merge > 0);
    assert(max_states >= max_states_before_merge);
    assert(shrink_threshold_before_merge <= max_states_before_merge);
}

void CPMAS::log_progress(const utils::Timer &timer, string msg) const {
    cout << "M&S algorithm timer: " << timer << " (" << msg << ")" << endl;
}

void CPMAS::report_peak_memory_delta(bool final) const {
    if (final)
        cout << "Final";
    else
        cout << "Current";
    cout << " peak memory increase of merge-and-shrink algorithm: "
         << utils::get_peak_memory_in_kb() - starting_peak_memory << " KB"
         << endl;
}

void CPMAS::dump_options() const {
    if (verbosity >= utils::Verbosity::NORMAL) {
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

        cout << "Main loop max time in seconds: " << main_loop_max_time << endl;
        cout << endl;
    }
}

void CPMAS::warn_on_unusual_options() const {
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

bool CPMAS::ran_out_of_time(
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
        cout << "Snapshot collector: next time: " << next_time_to_compute_snapshot
             << ", next iteration: " << next_iteration_to_compute_snapshot
             << endl;
    }
}

bool CPMAS::NextSnapshot::compute_next_snapshot(double current_time, int current_iteration) {
    if (!main_loop_target_num_snapshots && !main_loop_snapshot_each_iteration) {
        return false;
    }
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Snapshot collector: compute next snapshot? current time: " << current_time
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
            cout << "Compute snapshot now" << endl;
            cout << "Next snapshot: next time: " << next_time_to_compute_snapshot
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
    abstractions.push_back(utils::make_unique_ptr<Abstraction>(factor.first.release(), move(factor.second), unsolvable_index));
    return abstractions;
}

void add_cp_merge_and_shrink_algorithm_options_to_parser(OptionParser &parser) {
    add_merge_and_shrink_algorithm_options_to_parser(parser);

    // Cost partitioning options
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

    parser.add_option<bool>(
        "statistics_only",
        "If true, compute an OCP, an SCP, and the maximum over all factors "
        "after each transformation. Normalize values with the value of the "
        "atomic CP.",
        "false");
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
