#ifndef MERGE_AND_SHRINK_CP_MAS_OFFLINE_H
#define MERGE_AND_SHRINK_CP_MAS_OFFLINE_H

#include "cp_mas.h"

#include <set>

class TaskProxy;

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;

class CPMASOffline : public CPMAS {
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_over_fts_single_cp(
        const FactoredTransitionSystem &fts,
        const std::set<int> &indices) const;
    bool main_loop_single_cp(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy,
        std::vector<std::unique_ptr<Abstraction>> &abstractions,
        std::set<int> &factors_modified_since_last_snapshot,
        std::vector<std::vector<int>> &label_mappings,
        std::vector<int> &original_to_current_labels,
        std::vector<std::vector<int>> &reduced_to_original_labels);
public:
    explicit CPMASOffline(const options::Options &opts);
    std::unique_ptr<CostPartitioning> compute_single_ms_cp(
        const TaskProxy &task_proxy, CostPartitioningFactory &cp_factory);
};
}

#endif
