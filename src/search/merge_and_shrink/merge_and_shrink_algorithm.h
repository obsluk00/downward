#ifndef MERGE_AND_SHRINK_MERGE_AND_SHRINK_ALGORITHM_H
#define MERGE_AND_SHRINK_MERGE_AND_SHRINK_ALGORITHM_H

#include <functional>
#include <memory>

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
class FactoredTransitionSystem;
class LabelReduction;
class MergeStrategyFactory;
class ShrinkStrategy;

class FTSSnapshotCollector {
private:
    const bool compute_atomic_snapshot;
    const bool compute_final_snapshot;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;
    std::function<void (const FactoredTransitionSystem &fts)> handle_snapshot;
    utils::Verbosity verbosity;

    int num_main_loop_snapshots;
public:
    FTSSnapshotCollector(
        bool compute_atomic_snapshot,
        bool compute_final_snapshot,
        int main_loop_target_num_snapshots,
        int main_loop_snapshot_each_iteration,
        std::function<void (const FactoredTransitionSystem &fts)> handle_snapshot,
        utils::Verbosity verbosity);
    void report_atomic_snapshot(const FactoredTransitionSystem &fts);
    void report_main_loop_snapshot(
        const FactoredTransitionSystem &fts,
        double current_time,
        int current_iteration);
    void report_final_snapshot(const FactoredTransitionSystem &fts);

private:
    double max_time;
    int max_iterations;
    double next_time_to_compute_heuristic;
    int next_iteration_to_compute_heuristic;
    void compute_next_snapshot_time(double current_time);
    void compute_next_snapshot_iteration(int current_iteration);
    bool compute_next_snapshot(double current_time, int current_iteration);
public:
    void start_main_loop(double max_time, int max_iterations);
};

class MergeAndShrinkAlgorithm {
    // TODO: when the option parser supports it, the following should become
    // unique pointers.
    std::shared_ptr<MergeStrategyFactory> merge_strategy_factory;
    std::shared_ptr<ShrinkStrategy> shrink_strategy;
    std::shared_ptr<LabelReduction> label_reduction;

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

    long starting_peak_memory;

    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    void statistics(int maximum_intermediate_size) const;
    void main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        FTSSnapshotCollector *fts_snapshot_collector);
public:
    explicit MergeAndShrinkAlgorithm(const options::Options &opts);
    FactoredTransitionSystem build_factored_transition_system(
        const TaskProxy &task_proxy, FTSSnapshotCollector *fts_snapshot_collector = nullptr);
};

extern void add_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
extern void add_transition_system_size_limit_options_to_parser(options::OptionParser &parser);
extern void handle_shrink_limit_options_defaults(options::Options &opts);
}

#endif
