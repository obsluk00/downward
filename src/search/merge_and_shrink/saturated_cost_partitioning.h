#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

namespace utils {
class RandomNumberGenerator;
enum class Verbosity;
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
    const options::Options options;
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;
    const utils::Verbosity verbosity;

    std::unique_ptr<CostPartitioning> compute_scp_over_fts(
        const FactoredTransitionSystem &fts) const;
    SCPMSHeuristic extract_scp_heuristic(
        FactoredTransitionSystem &fts, int index) const;
public:
    explicit SaturatedCostPartitioningFactory(const options::Options &opts);
    virtual ~SaturatedCostPartitioningFactory() override;
    virtual std::vector<std::unique_ptr<CostPartitioning>> generate(
        const TaskProxy &task_proxy) const override;
};
}

#endif
