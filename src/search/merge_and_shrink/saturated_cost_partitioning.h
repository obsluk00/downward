#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include <vector>

namespace plugins {
class Options;
}

namespace merge_and_shrink {
class MergeAndShrinkRepresentation;
class OrderGenerator;

struct AbstractionInformation {
    std::vector<int> goal_distances;
    std::unique_ptr<MergeAndShrinkRepresentation> mas_representation;
};

class SaturatedCostPartitioning : public CostPartitioning {
    std::vector<AbstractionInformation> abstraction_infos;
public:
    explicit SaturatedCostPartitioning(
        std::vector<AbstractionInformation> &&abstraction_infos);
    virtual ~SaturatedCostPartitioning() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_abstractions() const override;
};

class SaturatedCostPartitioningFactory : public CostPartitioningFactory {
    std::shared_ptr<OrderGenerator> order_generator;
public:
    explicit SaturatedCostPartitioningFactory(const plugins::Options &opts);
    virtual ~SaturatedCostPartitioningFactory() = default;
    std::unique_ptr<CostPartitioning> generate_for_order(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        const std::vector<int> &order,
        utils::LogProxy &log) const;
    virtual std::unique_ptr<CostPartitioning> generate(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::LogProxy &log) override;
};
}

#endif