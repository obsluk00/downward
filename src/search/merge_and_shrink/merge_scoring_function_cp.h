#ifndef MERGE_AND_SHRINK_MERGE_SCORING_FUNCTION_CP_H
#define MERGE_AND_SHRINK_MERGE_SCORING_FUNCTION_CP_H

#include "merge_scoring_function.h"

#include <memory>

namespace plugins {
class Options;
}

namespace merge_and_shrink {
class Abstraction;
class CostPartitioningFactory;
class ShrinkStrategy;
class MergeScoringFunctionCP : public MergeScoringFunction {
    std::shared_ptr<ShrinkStrategy> shrink_strategy;
    const int max_states;
    const int max_states_before_merge;
    const int shrink_threshold_before_merge;
    std::shared_ptr<CostPartitioningFactory> cp_factory;
    bool filter_trivial_factors;
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_over_fts(
        const FactoredTransitionSystem &fts,
        const std::vector<int> &considered_factors) const;
protected:
    virtual std::string name() const override;
public:
    explicit MergeScoringFunctionCP(const plugins::Options &options);
    virtual ~MergeScoringFunctionCP() override = default;
    virtual std::vector<double> compute_scores(
        const FactoredTransitionSystem &fts,
        const std::vector<std::pair<int, int>> &merge_candidates) override;

    virtual bool requires_init_distances() const override {
        return true;
    }

    virtual bool requires_goal_distances() const override {
        return true;
    }
};
}

#endif
