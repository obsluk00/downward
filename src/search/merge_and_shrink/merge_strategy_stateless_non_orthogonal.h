#ifndef MERGE_AND_SHRINK_MERGE_STRATEGY_STATELESS_NON_ORTHOGONAL_H
#define MERGE_AND_SHRINK_MERGE_STRATEGY_STATELESS_NON_ORTHOGONAL_H

#include "merge_strategy.h"

#include <memory>
#include <vector>
#include <map>

namespace merge_and_shrink {
class MergeSelector;
class MergeStrategyStatelessNonOrthogonal : public MergeStrategy {
    const std::shared_ptr<MergeSelector> merge_selector;
    int tokens;
    std::vector<std::pair<int,int>> stored_merges;
    std::map<int,int> var_count;
    int times_to_clone;

public:
    MergeStrategyStatelessNonOrthogonal(
        const FactoredTransitionSystem &fts,
        const std::shared_ptr<MergeSelector> &merge_selector,
        int tokens);
    virtual ~MergeStrategyStatelessNonOrthogonal() override = default;
    virtual NextMerge get_next() override;
};
}

#endif
