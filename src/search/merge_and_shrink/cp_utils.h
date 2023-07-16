#ifndef MERGE_AND_SHRINK_CP_UTILS_H
#define MERGE_AND_SHRINK_CP_UTILS_H

#include <memory>
#include <vector>

namespace utils {
class LogProxy;
}

namespace merge_and_shrink {
struct Abstraction;
class FactoredTransitionSystem;
class Labels;

extern std::vector<int> compute_label_costs(const Labels &labels);

extern std::vector<std::unique_ptr<Abstraction>> compute_abstractions_for_factors(
    const FactoredTransitionSystem &fts,
    const std::vector<int> &considered_factors);
}

#endif
