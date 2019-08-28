#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

#include <vector>

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
    explicit SaturatedCostPartitioning(SCPMSHeuristic &&scp_ms_heuristic);
    virtual ~SaturatedCostPartitioning() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_factors() const override;
};

class SaturatedCostPartitioningFactory : public CostPartitioningFactory {
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;
public:
    explicit SaturatedCostPartitioningFactory(const Options &opts);
    virtual ~SaturatedCostPartitioningFactory() = default;
    virtual std::unique_ptr<CostPartitioning> generate_simple(
        const Labels &labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
    virtual std::unique_ptr<CostPartitioning> generate_over_different_labels(
        std::vector<int> &&original_labels,
        std::vector<int> &&label_costs,
        std::vector<std::vector<int>> &&label_mapping,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
};
}

#endif
