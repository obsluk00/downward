#ifndef MERGE_AND_SHRINK_CP_MAS_OFFLINE_H
#define MERGE_AND_SHRINK_CP_MAS_OFFLINE_H

#include "cp_mas.h"

#include <set>

class TaskProxy;

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;

class CPMASOffline : public CPMAS {
    std::vector<std::unique_ptr<Abstraction>> abstractions;
    void handle_snapshot(
        FactoredTransitionSystem &fts,
        const std::set<int> &factors_modified_since_last_snapshot,
        const std::vector<int> &original_to_current_labels,
        int unsolvable_index = -1);
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_over_fts(
        const FactoredTransitionSystem &fts,
        const std::set<int> &indices,
        const std::vector<int> &original_to_current_labels) const;
    void main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        std::set<int> &factors_modified_since_last_snapshot,
        std::vector<int> &original_to_current_labels);
public:
    explicit CPMASOffline(const options::Options &opts);
    virtual std::vector<std::unique_ptr<CostPartitioning>> compute_cps(
        const TaskProxy &task_proxy) override;
};
}

#endif
