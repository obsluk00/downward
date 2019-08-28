#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

#include <memory>
#include <vector>

class State;

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class Labels;
class MergeAndShrinkRepresentation;
class TransitionSystem;

class CostPartitioning {
public:
    CostPartitioning() = default;
    virtual ~CostPartitioning() = default;
    virtual int compute_value(const State &state) = 0;
    virtual int get_number_of_factors() const = 0;
    virtual void print_statistics() const {};
};

struct Abstraction {
    const TransitionSystem *transition_system;
    std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation;

    Abstraction(
        const TransitionSystem *transition_system,
        std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation);
    ~Abstraction();
};

class CostPartitioningFactory {
public:
    CostPartitioningFactory() = default;
    virtual ~CostPartitioningFactory() = default;
    virtual std::unique_ptr<CostPartitioning> generate(
        const Labels &labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) = 0;
};
}

#endif
