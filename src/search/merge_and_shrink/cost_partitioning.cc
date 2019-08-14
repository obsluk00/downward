#include "cost_partitioning.h"

#include "factored_transition_system.h"
#include "merge_and_shrink_algorithm.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/logging.h"

using namespace std;

namespace merge_and_shrink {
CostPartitioning::~CostPartitioning(){
}

CostPartitioningFactory::CostPartitioningFactory(const options::Options &opts)
    : options(opts),
      compute_atomic_snapshot(opts.get<bool>("compute_atomic_snapshot")),
      compute_final_snapshot(opts.get<bool>("compute_final_snapshot")),
      main_loop_target_num_snapshots(opts.get<int>("main_loop_target_num_snapshots")),
      main_loop_snapshot_each_iteration(opts.get<int>("main_loop_snapshot_each_iteration")),
      num_main_loop_snapshots(0),
      verbosity(static_cast<utils::Verbosity>(opts.get_enum("verbosity"))) {
}

void CostPartitioningFactory::compute_next_snapshot_time(double current_time) {
    int num_remaining_scp_heuristics = main_loop_target_num_snapshots - num_main_loop_snapshots;
    // safeguard against having aimed_num_scp_heuristics = 0
    if (num_remaining_scp_heuristics <= 0) {
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

void CostPartitioningFactory::compute_next_snapshot_iteration(int current_iteration) {
    if (main_loop_target_num_snapshots) {
        int num_remaining_scp_heuristics = main_loop_target_num_snapshots - num_main_loop_snapshots;
        // safeguard against having aimed_num_scp_heuristics = 0
        if (num_remaining_scp_heuristics <= 0) {
            next_iteration_to_compute_heuristic = max_iterations + 1;
            return;
        }
        int num_remaining_iterations = max_iterations - current_iteration;
        if (!num_remaining_iterations || num_remaining_scp_heuristics >= num_remaining_iterations) {
            next_iteration_to_compute_heuristic = current_iteration + 1;
            return;
        }
        double iteration_offset = num_remaining_iterations / static_cast<double>(num_remaining_scp_heuristics);
        assert(iteration_offset >= 1.0);
        next_iteration_to_compute_heuristic = current_iteration + static_cast<int>(iteration_offset);
    } else {
        next_iteration_to_compute_heuristic = current_iteration + main_loop_snapshot_each_iteration;
    }
}

bool CostPartitioningFactory::compute_next_snapshot(double current_time, int current_iteration) {
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
    if (current_time >= next_time_to_compute_heuristic ||
        current_iteration >= next_iteration_to_compute_heuristic) {
        compute = true;
    }
    if (compute) {
        compute_next_snapshot_time(current_time);
        compute_next_snapshot_iteration(current_iteration);
        if (verbosity == utils::Verbosity::DEBUG) {
            cout << "Compute snapshot now" << endl;
            cout << "Next snapshot: next time: " << next_time_to_compute_heuristic
                 << ", next iteration: " << next_iteration_to_compute_heuristic
                 << endl;
        }
    }
    return compute;
}

void CostPartitioningFactory::start_main_loop(double max_time, int max_iterations) {
    this->max_time = max_time;
    this->max_iterations = max_iterations;
    compute_next_snapshot_time(0);
    compute_next_snapshot_iteration(0);
    if (verbosity == utils::Verbosity::DEBUG) {
        cout << "Snapshot collector: next time: " << next_time_to_compute_heuristic
             << ", next iteration: " << next_iteration_to_compute_heuristic
             << endl;
    }
}

void CostPartitioningFactory::report_atomic_snapshot(const FactoredTransitionSystem &fts) {
    if (compute_atomic_snapshot) {
        cost_partitionings.push_back(handle_snapshot(fts));
    }
}

void CostPartitioningFactory::report_main_loop_snapshot(
    const FactoredTransitionSystem &fts,
    double current_time,
    int current_iteration) {
    if (compute_next_snapshot(current_time, current_iteration)) {
        cost_partitionings.push_back(handle_snapshot(fts));
        ++num_main_loop_snapshots;
    }
}

void CostPartitioningFactory::report_final_snapshot(const FactoredTransitionSystem &fts) {
    if (compute_final_snapshot) {
        cost_partitionings.push_back(handle_snapshot(fts));
    }
}

vector<unique_ptr<CostPartitioning>> &&CostPartitioningFactory::generate(
    const TaskProxy &task_proxy) {
    cout << "Generating cost partitionings using the merge-and-shrink algorithm..." << endl;
    MergeAndShrinkAlgorithm algorithm(options);
    FactoredTransitionSystem fts = algorithm.build_factored_transition_system(
        task_proxy, this);
    bool unsolvable = false;
    for (int index : fts) {
        if (!fts.is_factor_solvable(index)) {
            cost_partitionings.clear();
            cost_partitionings.reserve(1);
            cost_partitionings.push_back(handle_unsolvable_snapshot(fts, index));
            unsolvable= true;
            break;
        }
    }

    if (!unsolvable) {
        report_final_snapshot(fts);
    }

    if (cost_partitionings.empty()) {
        assert(!unsolvable);
        cost_partitionings.push_back(handle_snapshot(fts));
    }

    int num_cps = cost_partitionings.size();
    cout << "Number of cost partitionings: " << num_cps << endl;
    cout << "Done generating cost partitionings." << endl << endl;
    return move(cost_partitionings);
}

void add_cost_partitioning_factory_options_to_parser(OptionParser &parser) {
    add_merge_and_shrink_algorithm_options_to_parser(parser);
    parser.add_option<bool>(
        "compute_atomic_snapshot",
        "Include an SCP heuristic computed over the atomic FTS.",
        "false");
    parser.add_option<bool>(
        "compute_final_snapshot",
        "Include an SCP heuristic computed over the final FTS (attention: "
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
}

void handle_cost_partitioning_factory_options(Options &opts) {
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

static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
