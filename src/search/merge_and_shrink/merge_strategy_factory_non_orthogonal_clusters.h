#ifndef MERGE_AND_SHRINK_MERGE_STRATEGY_FACTORY_NON_ORTHOGONAL_CLUSTERS_H
#define MERGE_AND_SHRINK_MERGE_STRATEGY_FACTORY_NON_ORTHOGONAL_CLUSTERS_H

#include "merge_strategy_factory.h"

namespace merge_and_shrink {
    class MergeTreeFactory;
    class MergeSelector;

    // How to distribute tokens
    enum class CloneStrategy {
        TOTAL,
        COMBINE_SMALLEST,
        COMBINE_LARGEST
    };

    // How to create clusters
    // TODO: new class for this
    enum class ClusterStrategy {
        PREDECESSORS,
        SUCCESSORS,
        BOTH
    };

    class MergeStrategyFactoryNonOrthogonalClusters : public MergeStrategyFactory {
        CloneStrategy clone_strategy;
        ClusterStrategy cluster_strategy;
        int tokens;
        std::shared_ptr<MergeSelector> merge_selector;
    protected:
        virtual std::string name() const override;
        virtual void dump_strategy_specific_options() const override;
    public:
        explicit MergeStrategyFactoryNonOrthogonalClusters(const plugins::Options &options);
        virtual ~MergeStrategyFactoryNonOrthogonalClusters() override = default;
        virtual std::unique_ptr<MergeStrategy> compute_merge_strategy(
                const TaskProxy &task_proxy,
                const FactoredTransitionSystem &fts) override;
        virtual bool requires_init_distances() const override;
        virtual bool requires_goal_distances() const override;
    };
}

#endif
