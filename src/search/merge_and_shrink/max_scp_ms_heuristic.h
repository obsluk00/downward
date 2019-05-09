#ifndef MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H
#define MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H

#include "../heuristic.h"

#include "merge_and_shrink_algorithm.h"

namespace merge_and_shrink {
class MaxSCPMSHeuristic : public Heuristic {
    SCPMSHeuristics scp_ms_heuristics;
protected:
    virtual int compute_heuristic(const GlobalState &global_state) override;
public:
    explicit MaxSCPMSHeuristic(const options::Options &opts);
};
}

#endif
