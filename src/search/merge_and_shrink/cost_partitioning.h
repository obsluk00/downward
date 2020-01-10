#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

#include <memory>
#include <vector>

class State;
class TaskProxy;

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
    virtual void print_statistics() const {}
};

struct Abstraction {
    const TransitionSystem *transition_system;
    std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation;
    int fts_index;

    Abstraction(
        const TransitionSystem *transition_system,
        std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation,
        int fts_index);
    ~Abstraction();
};

class CostPartitioningFactory {
public:
    CostPartitioningFactory() = default;
    virtual ~CostPartitioningFactory() = default;
    virtual void initialize(const TaskProxy &) {}
    virtual std::unique_ptr<CostPartitioning> generate_simple(
        const Labels &labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) = 0;
    virtual std::unique_ptr<CostPartitioning> generate_over_different_labels(
        std::vector<int> &&original_labels,
        std::vector<int> &&label_costs,
        std::vector<std::vector<int>> &&label_mappings,
        std::vector<std::vector<int>> &&reduced_to_original_labels,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) = 0;
};
}

#endif
