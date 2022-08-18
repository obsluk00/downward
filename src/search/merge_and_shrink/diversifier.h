#ifndef MERGE_AND_SHRINK_DIVERSIFIER_H
#define MERGE_AND_SHRINK_DIVERSIFIER_H

#include "types.h"

namespace merge_and_shrink {
class CostPartitioningHeuristic;

class Diversifier {
    std::vector<std::vector<int>> abstract_state_ids_by_sample;
    std::vector<int> portfolio_h_values;

public:
    explicit Diversifier(std::vector<std::vector<int>> &&abstract_state_ids_by_sample);

    /* Return true iff the cost-partitioned heuristic has a higher heuristic
       value than all previously seen heuristics for at least one sample. */
    bool is_diverse(const CostPartitioningHeuristic &cp_heuristic);

    float compute_avg_finite_sample_h_value() const;
};
}

#endif
