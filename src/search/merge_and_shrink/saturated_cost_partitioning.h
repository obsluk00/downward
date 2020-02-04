#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

#include <vector>

namespace merge_and_shrink {
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;
class SingleUseOrderGenerator;

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
    std::shared_ptr<SingleUseOrderGenerator> order_generator;
    std::unique_ptr<CostPartitioning> generate_for_order(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        const std::vector<int> order,
        utils::Verbosity verbosity) const;
public:
    explicit SaturatedCostPartitioningFactory(const Options &opts);
    virtual ~SaturatedCostPartitioningFactory() = default;
    virtual void initialize(const TaskProxy &task_proxy) override;
    virtual std::unique_ptr<CostPartitioning> generate(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
};
}

#endif
