#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_UTILS_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONING_UTILS_H

#include <vector>

namespace utils {
class LogProxy;
}

namespace merge_and_shrink {
class Abstraction;

extern std::vector<int> compute_goal_distances_for_abstraction(
    const Abstraction &abstraction, const std::vector<int> &label_costs, utils::LogProxy &log);
extern std::vector<std::vector<int>> compute_inverse_label_mapping(const Abstraction &abstraction);
extern std::vector<int> compute_saturated_costs_for_abstraction(
    const Abstraction &abstraction,
    const std::vector<int> &goal_distances,
    int num_labels,
    utils::LogProxy &log);
extern void reduce_costs(std::vector<int> &label_costs, const std::vector<int> &saturated_label_costs);
}
#endif
