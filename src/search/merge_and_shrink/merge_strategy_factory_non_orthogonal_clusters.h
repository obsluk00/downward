#ifndef MERGE_AND_SHRINK_MERGE_STRATEGY_FACTORY_NON_ORTHOGONAL_CLUSTERS_H
#define MERGE_AND_SHRINK_MERGE_STRATEGY_FACTORY_NON_ORTHOGONAL_CLUSTERS_H

#include "merge_strategy_factory.h"

#include "../utils/rng_options.h"

#include <map>

namespace utils {
    class RandomNumberGenerator;
}

namespace merge_and_shrink {
    class MergeTreeFactory;
    class MergeSelector;

    // How to distribute tokens
    enum class CombineStrategy {
        TOTAL,
        COMBINE_SMALLEST,
        COMBINE_LARGEST,
        RANDOM,
        LARGEST_OVERLAP
    };

    // How to create clusters
    // TODO: new class for this
    enum class ClusterStrategy {
        PREDECESSORS,
        SUCCESSORS,
        BOTH
    };

    class MergeStrategyFactoryNonOrthogonalClusters : public MergeStrategyFactory {
        const std::shared_ptr<utils::RandomNumberGenerator> rng;
        CombineStrategy combine_strategy;
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
    private:
        int compute_times_to_clone(std::map<int, int> var_count, int variable_count);
        std::map<int, int> compute_var_count(std::vector<std::vector<int>> clusters, const TaskProxy &task_proxy);
        std::vector<std::vector<int>> combine_clusters(std::vector<std::vector<int>> clusters, CombineStrategy combine_strategy);
    };
}

#endif
