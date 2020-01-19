#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

#include <vector>

namespace merge_and_shrink {
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;
class MASOrderGenerator;

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
};

class SaturatedCostPartitioning : public CostPartitioning {
    SCPMSHeuristic scp_ms_heuristic;
public:
    explicit SaturatedCostPartitioning(SCPMSHeuristic &&scp_ms_heuristic);
    virtual ~SaturatedCostPartitioning() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_factors() const override;
};

class SaturatedCostPartitioningFactory : public CostPartitioningFactory {
    std::shared_ptr<MASOrderGenerator> order_generator;
    std::vector<int> compute_saturated_costs_simple(
        const TransitionSystem &ts,
        const std::vector<int> &goal_distances,
        int num_labels,
        utils::Verbosity verbosity) const;
    std::vector<int> compute_goal_distances_different_labels(
        const TransitionSystem &ts,
        int num_original_labels,
        const std::vector<int> &remaining_label_costs,
        const std::vector<int> &label_mapping,
        utils::Verbosity verbosity) const;
    std::vector<int> compute_saturated_costs_different_labels(
        const TransitionSystem &ts,
        const std::vector<int> &goal_distances,
        int num_original_labels,
        const std::vector<std::vector<int>> &reduced_to_original_labels,
        const std::vector<int> &remaining_label_costs,
        utils::Verbosity verbosity) const;
public:
    explicit SaturatedCostPartitioningFactory(const Options &opts);
    virtual ~SaturatedCostPartitioningFactory() = default;
    virtual void initialize(const TaskProxy &task_proxy) override;
    virtual std::unique_ptr<CostPartitioning> generate_simple(
        const Labels &labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
    virtual std::unique_ptr<CostPartitioning> generate_over_different_labels(
        std::vector<int> &&original_labels,
        std::vector<int> &&label_costs,
        std::vector<std::vector<int>> &&label_mappings,
        std::vector<std::vector<int>> &&reduced_to_original_labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
};
}

#endif
