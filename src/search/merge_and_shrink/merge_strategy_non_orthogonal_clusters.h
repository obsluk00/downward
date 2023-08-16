#ifndef MERGE_AND_SHRINK_MERGE_STRATEGY_NON_ORTHOGONAL_CLUSTERS_H
#define MERGE_AND_SHRINK_MERGE_STRATEGY_NON_ORTHOGONAL_CLUSTERS_H

#include "merge_strategy.h"

#include <memory>
#include <vector>
#include <map>

class TaskProxy;

namespace merge_and_shrink {
class MergeSelector;
class MergeTreeFactory;
class MergeTree;
class MergeStrategyNonOrthogonalClusters : public MergeStrategy {
    const TaskProxy &task_proxy;
    std::shared_ptr<MergeSelector> merge_selector;
    std::vector<std::vector<int>> clusters;
    int tokens;
    // Active "merge strategies" while merging a set of indices
    std::vector<int> current_ts_indices;
    std::map<int,int> var_count;

public:
    MergeStrategyNonOrthogonalClusters(
        const FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        const std::shared_ptr<MergeSelector> &merge_selector,
        std::vector<std::vector<int>> clusters,
        std::map<int,int> var_count,
        int tokens);
    virtual ~MergeStrategyNonOrthogonalClusters() override;
    virtual NextMerge get_next() override;
};
}

#endif
