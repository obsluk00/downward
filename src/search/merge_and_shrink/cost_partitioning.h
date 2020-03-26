#ifndef MERGE_AND_SHRINK_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_COST_PARTITIONING_H

#include <memory>
#include <vector>

class AbstractTask;
class State;

namespace options {
class OptionParser;
class Options;
}

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
    virtual int get_number_of_abstractions() const = 0;
};

struct Abstraction {
    /*
      NOTE: depending on the use case, this is either a copy owned by this
      class (which therefore needs to be deleted at the end), or it is a
      pointer to a transition system in some FTS which should not be deleted.
      TODO: can we deal with this in a nicer way?
    */
    const TransitionSystem *transition_system;
    std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation;
    const std::vector<int> label_mapping;

    Abstraction(
        const TransitionSystem *transition_system,
        std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation,
        const std::vector<int> &label_mapping = std::vector<int>());
    ~Abstraction();
    std::unique_ptr<MergeAndShrinkRepresentation> extract_abstraction_function();
};

class CostPartitioningFactory {
public:
    virtual ~CostPartitioningFactory() = default;
    virtual void initialize(const std::shared_ptr<AbstractTask> &) {}
    virtual std::unique_ptr<CostPartitioning> generate(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) = 0;
};

extern void add_cp_options_to_parser(options::OptionParser &parser);
}

#endif
