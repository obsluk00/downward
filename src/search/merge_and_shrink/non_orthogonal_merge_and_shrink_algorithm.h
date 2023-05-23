#ifndef MERGE_AND_SHRINK_NON_ORTHOGONAL_MERGE_AND_SHRINK_ALGORITHM_H
#define MERGE_AND_SHRINK_NON_ORTHOGONAL_MERGE_AND_SHRINK_ALGORITHM_H

#include "../utils/logging.h"
#include "merge_and_shrink_algorithm.h"

#include <memory>

class TaskProxy;

namespace plugins {
class ConstructContext;
class Feature;
class Options;
}

namespace utils {
class CountdownTimer;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class LabelReduction;
class MergeStrategyFactory;
class ShrinkStrategy;

class NonOrthogonalMergeAndShrinkAlgorithm {
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

    // TODO: Options for cloning
    const bool non_orthogonal;

    mutable utils::LogProxy log;
    const double main_loop_max_time;

    long starting_peak_memory;

    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    void statistics(int maximum_intermediate_size) const;
    void main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy);
public:
    explicit NonOrthogonalMergeAndShrinkAlgorithm(const plugins::Options &opts);
    FactoredTransitionSystem build_factored_transition_system(const TaskProxy &task_proxy);
};

}

#endif
