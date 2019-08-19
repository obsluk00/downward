#ifndef MERGE_AND_SHRINK_CP_MERGE_AND_SHRINK_ALGORITHM_H
#define MERGE_AND_SHRINK_CP_MERGE_AND_SHRINK_ALGORITHM_H

#include <functional>
#include <memory>
#include <vector>

class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class CountdownTimer;
enum class Verbosity;
}

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;
class FactoredTransitionSystem;
class LabelReduction;
class MergeStrategyFactory;
class ShrinkStrategy;

class CPMergeAndShrinkAlgorithm {
    // TODO: when the option parser supports it, the following should become
    // unique pointers.
    std::shared_ptr<MergeStrategyFactory> merge_strategy_factory;
    std::shared_ptr<ShrinkStrategy> shrink_strategy;
    std::shared_ptr<LabelReduction> label_reduction;
    std::shared_ptr<CostPartitioningFactory> cp_factory;

    // Options for shrinking
    // Hard limit: the maximum size of a transition system at any point.
    const int max_states;
    // Hard limit: the maximum size of a transition system before being merged.
    const int max_states_before_merge;
    /* A soft limit for triggering shrinking even if the hard limits
       max_states and max_states_before_merge are not violated. */
    const int shrink_threshold_before_merge;

    // Options for pruning
    const bool prune_unreachable_states;
    const bool prune_irrelevant_states;

    const utils::Verbosity verbosity;
    const double main_loop_max_time;

    const bool compute_atomic_snapshot;
    const bool compute_final_snapshot;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;

    long starting_peak_memory;

    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    void statistics(int maximum_intermediate_size) const;
    bool main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        std::vector<std::unique_ptr<CostPartitioning>> &cost_partitionings);
public:
    explicit CPMergeAndShrinkAlgorithm(const options::Options &opts);
    std::vector<std::unique_ptr<CostPartitioning>> compute_ms_cps(
        const TaskProxy &task_proxy);
};

extern void add_cp_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
extern void handle_cp_merge_and_shrink_algorithm_options(options::Options &opts);
}

#endif
