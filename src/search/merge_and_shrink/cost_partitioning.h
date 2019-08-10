#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

class State;
class TaskProxy;

#include <memory>
#include <vector>

namespace merge_and_shrink {
class CostPartitioning {
public:
    CostPartitioning() = default;
    virtual ~CostPartitioning() = 0;
    virtual int compute_value(const State &state) = 0;
};

class CostPartitioningFactory {
public:
    CostPartitioningFactory() = default;
    virtual ~CostPartitioningFactory() = 0;
    virtual std::vector<std::unique_ptr<CostPartitioning>> generate(
        const TaskProxy &task_proxy) const = 0;
};
}

#endif
