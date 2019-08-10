#ifndef MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H
#define MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H

#include "../heuristic.h"

namespace merge_and_shrink {
class CostPartitioning;
class CostPartitioningFactory;

class MaxSCPMSHeuristic : public Heuristic {
    std::vector<std::unique_ptr<CostPartitioning>> cost_partitionings;
protected:
    virtual int compute_heuristic(const GlobalState &global_state) override;
public:
    explicit MaxSCPMSHeuristic(const options::Options &opts);
};
}

#endif
