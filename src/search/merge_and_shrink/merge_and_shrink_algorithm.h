#ifndef MERGE_AND_SHRINK_MERGE_AND_SHRINK_ALGORITHM_H
#define MERGE_AND_SHRINK_MERGE_AND_SHRINK_ALGORITHM_H

#include <memory>
#include <vector>

class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class CountdownTimer;
class RandomNumberGenerator;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class LabelReduction;
class MergeAndShrinkRepresentation;
class MergeStrategyFactory;
class ShrinkStrategy;
enum class Verbosity;

enum class FactorOrder {
    GIVEN,
    RANDOM
};

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
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

    const Verbosity verbosity;
    const double main_loop_max_time;
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;
    const bool scp_over_atomic_fts;
    const bool scp_over_final_fts;
    const int main_loop_num_scp_heuristics;
    const int main_loop_iteration_offset_for_computing_scp_heuristics;

    long starting_peak_memory;

    void report_peak_memory_delta(bool final = false) const;
    void dump_options() const;
    void warn_on_unusual_options() const;
    bool ran_out_of_time(const utils::CountdownTimer &timer) const;
    void statistics(int maximum_intermediate_size) const;
    SCPMSHeuristic extract_scp_heuristic(
        FactoredTransitionSystem &fts, int index) const;
    bool main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        std::vector<SCPMSHeuristic> *scp_ms_heuristics = nullptr);
    SCPMSHeuristic compute_scp_ms_heuristic_over_fts(
        const FactoredTransitionSystem &fts) const;
public:
    explicit MergeAndShrinkAlgorithm(const options::Options &opts);
    std::vector<SCPMSHeuristic> compute_scp_ms_heuristics(const TaskProxy &task_proxy);
    FactoredTransitionSystem build_factored_transition_system(const TaskProxy &task_proxy);
};

extern void add_merge_and_shrink_algorithm_options_to_parser(options::OptionParser &parser);
extern void add_transition_system_size_limit_options_to_parser(options::OptionParser &parser);
extern void handle_shrink_limit_options_defaults(options::Options &opts);
}

#endif
