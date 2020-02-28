#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

#include <vector>

namespace merge_and_shrink {
class MergeAndShrinkRepresentation;
class SingleUseOrderGenerator;

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
    virtual int get_number_of_factors() const override;
};

class SaturatedCostPartitioningFactory : public CostPartitioningFactory {
    std::shared_ptr<SingleUseOrderGenerator> single_use_order_generator;
    std::unique_ptr<CostPartitioning> generate_for_order(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        const std::vector<int> &order,
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
