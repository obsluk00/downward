#ifndef MERGE_AND_SHRINK_CP_MAS_H
#define MERGE_AND_SHRINK_CP_MAS_H

#include "../algorithms/dynamic_bitset.h"

#include <memory>
#include <vector>

class AbstractTask;
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
class Abstraction;
class CostPartitioning;
class CostPartitioningFactory;
class FactoredTransitionSystem;
class LabelReduction;
class Labels;
class MergeStrategyFactory;
class ShrinkStrategy;

using Bitset = dynamic_bitset::DynamicBitset<unsigned short>;

class CPMAS {
protected:
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
    const bool atomic_label_reduction;

    // Options for cost partitioning
    const bool compute_atomic_snapshot;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;

    enum class SnapshotMoment {
        AFTER_LABEL_REDUCTION,
        AFTER_SHRINKING,
        AFTER_MERGING,
        AFTER_PRUNING
    };
    const SnapshotMoment snapshot_moment;
    const bool filter_trivial_factors;
    const bool statistics_only;

    const bool offline_cps;
    // Used if offline_cps = true
    std::vector<std::unique_ptr<Abstraction>> abstractions;
    // Used if offline_cps = false
    std::vector<std::unique_ptr<CostPartitioning>> cost_partitionings;

    std::shared_ptr<CostPartitioningFactory> cp_factory;

    long starting_peak_memory;

    class NextSnapshot {
    private:
        const double max_time;
        const int max_iterations;
        const int main_loop_target_num_snapshots;
        const int main_loop_snapshot_each_iteration;
        const utils::Verbosity verbosity;

        double next_time_to_compute_snapshot;
        int next_iteration_to_compute_snapshot;
        int num_main_loop_snapshots;

        void compute_next_snapshot_time(double current_time);
        void compute_next_snapshot_iteration(int current_iteration);
    public:
        /*
          Counting of iterations is 1-based in this class.
        */
        NextSnapshot(
            double max_time,
            int max_iterations,
            int main_loop_target_num_snapshots,
            int main_loop_snapshot_each_iteration,
            utils::Verbosity verbosity);

        bool compute_next_snapshot(double current_time, int current_iteration);
    };

    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    std::vector<int> compute_label_costs(const Labels &labels) const;
    std::vector<std::unique_ptr<Abstraction>> extract_unsolvable_abstraction(
        FactoredTransitionSystem &fts, int unsolvable_index) const;
    void handle_unsolvable_snapshot(
        FactoredTransitionSystem &fts, int unsolvable_index);
    void handle_snapshot(
        const FactoredTransitionSystem &fts,
        Bitset &factors_modified_since_last_snapshot,
        const std::unique_ptr<std::vector<int>> &original_to_current_labels);
    // TODO: the following two methods could be split further and party combined.
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_for_interleaved_cp(
        const FactoredTransitionSystem &fts) const;
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_for_offline_cp(
        const FactoredTransitionSystem &fts,
        const Bitset &factors_modified_since_last_snapshot,
        const std::vector<int> &original_to_current_labels) const;
    void compute_cp_and_print_statistics(
        const FactoredTransitionSystem &fts,
        int iteration) const;
    bool main_loop(FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        Bitset &factors_modified_since_last_snapshot,
        const std::unique_ptr<std::vector<int>> &original_to_current_labels);
public:
    explicit CPMAS(const options::Options &opts);
    ~CPMAS() = default;
    std::vector<std::unique_ptr<CostPartitioning>> compute_cps(
        const std::shared_ptr<AbstractTask> &task);
};

extern void add_cp_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
extern void handle_cp_merge_and_shrink_algorithm_options(options::Options &opts);
}

#endif
