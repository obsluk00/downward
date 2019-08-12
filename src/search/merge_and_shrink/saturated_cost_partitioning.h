#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

namespace utils {
class RandomNumberGenerator;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;

enum class FactorOrder {
    GIVEN,
    RANDOM
};

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
};

class SaturatedCostPartitioning : public CostPartitioning {
    SCPMSHeuristic scp_ms_heuristic;
public:
    explicit SaturatedCostPartitioning(SCPMSHeuristic scp_ms_heuristic);
    virtual ~SaturatedCostPartitioning() override;
    virtual int compute_value(const State &state) override;
};

class SaturatedCostPartitioningFactory : public CostPartitioningFactory {
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;

    SCPMSHeuristic extract_scp_heuristic(
        FactoredTransitionSystem &fts, int index) const;
protected:
    virtual std::unique_ptr<CostPartitioning> handle_snapshot(
        const FactoredTransitionSystem &fts) override;
    virtual std::unique_ptr<CostPartitioning> handle_unsolvable_snapshot(
        FactoredTransitionSystem &fts, int index) override;
public:
    explicit SaturatedCostPartitioningFactory(const options::Options &opts);
    virtual ~SaturatedCostPartitioningFactory() override;
    virtual std::vector<std::unique_ptr<CostPartitioning>> generate(
        const TaskProxy &task_proxy) override;
};
}

#endif
