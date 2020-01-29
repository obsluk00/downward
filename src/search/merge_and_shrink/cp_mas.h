#ifndef MERGE_AND_SHRINK_CP_MAS_H
#define MERGE_AND_SHRINK_CP_MAS_H

#include <memory>
#include <vector>

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class CountdownTimer;
class Timer;
enum class Verbosity;
}

namespace merge_and_shrink {
class Abstraction;
class FactoredTransitionSystem;
class LabelReduction;
class Labels;
class MergeStrategyFactory;
class ShrinkStrategy;

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

    const bool compute_atomic_snapshot;
    const bool compute_final_snapshot;
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

    void log_progress(const utils::Timer &timer, std::string msg) const;
    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    std::vector<int> compute_label_costs(const Labels &labels) const;
    std::vector<std::unique_ptr<Abstraction>> extract_unsolvable_abstraction(
        FactoredTransitionSystem &fts, int unsolvable_index) const;
public:
    explicit CPMAS(const options::Options &opts);
};

extern void add_cp_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
extern void handle_cp_merge_and_shrink_algorithm_options(options::Options &opts);
}

#endif
