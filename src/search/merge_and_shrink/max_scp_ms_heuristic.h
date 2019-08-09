#ifndef MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H
#define MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H

#include "../heuristic.h"

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;

enum class FactorOrder {
    GIVEN,
    RANDOM
};

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
};

class MaxSCPMSHeuristic : public Heuristic {
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;
    const utils::Verbosity verbosity;

    std::vector<SCPMSHeuristic> scp_ms_heuristics;

    SCPMSHeuristic extract_scp_heuristic(
        FactoredTransitionSystem &fts, int index) const;
    SCPMSHeuristic compute_scp_ms_heuristic_over_fts(
        const FactoredTransitionSystem &fts) const;
protected:
    virtual int compute_heuristic(const GlobalState &global_state) override;
public:
    explicit MaxSCPMSHeuristic(const options::Options &opts);
};
}

#endif
