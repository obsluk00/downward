#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

#include <memory>

class State;

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;

class CostPartitioning {
public:
    CostPartitioning() = default;
    virtual ~CostPartitioning() = default;
    virtual int compute_value(const State &state) = 0;
    virtual int get_number_of_factors() const = 0;
    virtual void print_statistics() const {};
};

class CostPartitioningFactory {
public:
    CostPartitioningFactory() = default;
    virtual ~CostPartitioningFactory() = default;
    virtual std::unique_ptr<CostPartitioning> generate(
        FactoredTransitionSystem &fts,
        utils::Verbosity verbosity,
        int unsolvable_index = -1) = 0;
};
}

#endif
