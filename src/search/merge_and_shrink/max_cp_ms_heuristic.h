#ifndef MERGE_AND_SHRINK_MAX_CP_MS_HEURISTIC_H
#define MERGE_AND_SHRINK_MAX_CP_MS_HEURISTIC_H

#include "../heuristic.h"

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;

class MaxCPMSHeuristic : public Heuristic {
    std::vector<std::unique_ptr<CostPartitioning>> cost_partitionings;
protected:
    virtual int compute_heuristic(const GlobalState &global_state) override;
public:
    explicit MaxCPMSHeuristic(const options::Options &opts);
};
}

#endif
