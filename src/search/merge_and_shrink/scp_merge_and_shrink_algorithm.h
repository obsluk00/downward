#ifndef MERGE_AND_SHRINK_SCP_MERGE_AND_SHRINK_ALGORITHM_H
#define MERGE_AND_SHRINK_SCP_MERGE_AND_SHRINK_ALGORITHM_H

#include <memory>
#include <vector>

class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class CountdownTimer;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class LabelReduction;
class MergeAndShrinkRepresentation;
class MergeStrategyFactory;
class ShrinkStrategy;
enum class Verbosity;

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<const MergeAndShrinkRepresentation *> mas_representation_raw_ptrs;
};

struct SCPMSHeuristics {
    std::vector<SCPMSHeuristic> scp_ms_heuristics;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
};

class SCPMergeAndShrinkAlgorithm {
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

    const Verbosity verbosity;
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
        std::vector<SCPMSHeuristic> &scp_ms_heuristics);
    SCPMSHeuristic compute_scp_ms_heuristic_over_fts(
        const FactoredTransitionSystem &fts) const;
public:
    explicit SCPMergeAndShrinkAlgorithm(const options::Options &opts);
    SCPMSHeuristics compute_scp_ms_heuristics(const TaskProxy &task_proxy);
};

extern void add_scp_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
}

#endif
