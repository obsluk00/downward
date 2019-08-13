#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

#include "../option_parser.h"

#include <memory>
#include <vector>

class State;
class TaskProxy;

namespace options {
class Options;
}

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;

class CostPartitioning {
public:
    CostPartitioning() = default;
    virtual ~CostPartitioning() = 0;
    virtual int compute_value(const State &state) = 0;
};

class CostPartitioningFactory {
    const Options options;
    const bool compute_atomic_snapshot;
    const bool compute_final_snapshot;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;

    double max_time;
    int max_iterations;
    double next_time_to_compute_heuristic;
    int next_iteration_to_compute_heuristic;

    std::vector<std::unique_ptr<CostPartitioning>> cost_partitionings;
    int num_main_loop_snapshots;

    void compute_next_snapshot_time(double current_time);
    void compute_next_snapshot_iteration(int current_iteration);
    bool compute_next_snapshot(double current_time, int current_iteration);
protected:
    const utils::Verbosity verbosity;

    virtual std::unique_ptr<CostPartitioning> handle_snapshot(
        const FactoredTransitionSystem &fts) = 0;
    virtual std::unique_ptr<CostPartitioning> handle_unsolvable_snapshot(
        FactoredTransitionSystem &fts, int index) = 0;
public:
    explicit CostPartitioningFactory(const options::Options &opts);
    virtual ~CostPartitioningFactory() = default;
    void start_main_loop(double max_time, int max_iterations);
    void report_atomic_snapshot(const FactoredTransitionSystem &fts);
    void report_main_loop_snapshot(
        const FactoredTransitionSystem &fts,
        double current_time,
        int current_iteration);
    void report_final_snapshot(const FactoredTransitionSystem &fts);
    virtual std::vector<std::unique_ptr<CostPartitioning>> generate(
        const TaskProxy &task_proxy);
};

extern void add_cost_partitioning_factory_options_to_parser(OptionParser &parser);
extern void handle_cost_partitioning_factory_options(Options &opts);
}

#endif
