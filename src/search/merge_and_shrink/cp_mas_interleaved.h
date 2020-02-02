#ifndef MERGE_AND_SHRINK_CP_MAS_INTERLEAVED_H
#define MERGE_AND_SHRINK_CP_MAS_INTERLEAVED_H

#include "cp_mas.h"

class TaskProxy;

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;

class CPMASInterleaved : public CPMAS {
    std::vector<std::unique_ptr<CostPartitioning>> cost_partitionings;
    void compute_cp_and_print_statistics(
        const FactoredTransitionSystem &fts,
        int iteration) const;
    void handle_snapshot(
        FactoredTransitionSystem &fts,
        int unsolvable_index = -1);
    std::vector<std::unique_ptr<Abstraction>> compute_abstractions_over_fts(
        const FactoredTransitionSystem &fts) const;
    bool main_loop(
        FactoredTransitionSystem &fts,
        const TaskProxy &task_proxy);
public:
    explicit CPMASInterleaved(const options::Options &opts);
    virtual std::vector<std::unique_ptr<CostPartitioning>> compute_cps(
        const TaskProxy &task_proxy) override;
};
}

#endif
