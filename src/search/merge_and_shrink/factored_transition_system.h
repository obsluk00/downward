#ifndef MERGE_AND_SHRINK_FACTORED_TRANSITION_SYSTEM_H
#define MERGE_AND_SHRINK_FACTORED_TRANSITION_SYSTEM_H

#include "types.h"

#include <memory>
#include <vector>

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class Distances;
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;
class Labels;
class TransitionSystem;

class FTSConstIterator {
    /*
      This class allows users to easily iterate over the active indices of
      a factored transition system.
    */
    const FactoredTransitionSystem &fts;
    // current_index is the actual iterator
    int current_index;

    void next_valid_index();
public:
    FTSConstIterator(const FactoredTransitionSystem &fts, bool end);
    void operator++();

    int operator*() const {
        return current_index;
    }

    bool operator==(const FTSConstIterator &rhs) const {
        return current_index == rhs.current_index;
    }

    bool operator!=(const FTSConstIterator &rhs) const {
        return current_index != rhs.current_index;
    }
};

/*
  NOTE: A "factor" of this factored transition system is identfied by its
  index as used in the vectors in this class. Since transformations like
  merging also add and remove factors, not all indices are necessarily
  associated with factors. This is what the class uses the notion of "active"
  factors for: an index is active iff there exists a transition system, a
  merge-and-shrink representation and an distances object in the corresponding
  vectors.

  TODO: The user of this class has to care more about the notion of active
  factors as we would like it to be. We should change this and clean up the
  interface that this class shows to the outside world.
*/
class FactoredTransitionSystem {
    std::unique_ptr<Labels> labels;
    // Entries with nullptr have been merged.
    std::vector<std::unique_ptr<TransitionSystem>> transition_systems;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
    std::vector<std::unique_ptr<Distances>> distances;
    const bool compute_init_distances;
    const bool compute_goal_distances;
    int num_active_entries;

    /*
      Assert that the factor at the given index is in a consistent state, i.e.
      that there is a transition system, a distances object, and an MSR.
    */
    void assert_index_valid(int index) const;

    /*
      We maintain the invariant that for all factors, distances are always
      computed and all transitions are grouped according to locally equivalent
      labels.
    */
    bool is_component_valid(int index) const;

    void assert_all_components_valid() const;
public:
    FactoredTransitionSystem(
        std::unique_ptr<Labels> labels,
        std::vector<std::unique_ptr<TransitionSystem>> &&transition_systems,
        std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> &&mas_representations,
        std::vector<std::unique_ptr<Distances>> &&distances,
        bool compute_init_distances,
        bool compute_goal_distances,
        utils::Verbosity verbosity);
    FactoredTransitionSystem(FactoredTransitionSystem &&other);
    ~FactoredTransitionSystem();

    // No copying or assignment.
    FactoredTransitionSystem(const FactoredTransitionSystem &) = delete;
    FactoredTransitionSystem &operator=(
        const FactoredTransitionSystem &) = delete;

    // Merge-and-shrink transformations.
    /*
      Apply the given label mapping to the factored transition system by
      updating all transitions of all transition systems. Only for the factor
      at combinable_index, the local equivalence relation over labels must be
      recomputed; for all factors, all labels that are combined by the label
      mapping have been locally equivalent already before.
    */
    void apply_label_mapping(
        const std::vector<std::pair<int, std::vector<int>>> &label_mapping,
        int combinable_index);

    /*
      Apply the given state equivalence relation to the transition system at
      index if it would reduce its size. If the transition system was shrunk,
      update the other components of the factor (distances, MSR) and return
      true, otherwise return false.

      Note that this method is also suitable to be used for a prune
      transformation. All states not mentioned in the state equivalence
      relation are pruned.
    */
    bool apply_abstraction(
        int index,
        const StateEquivalenceRelation &state_equivalence_relation,
        utils::Verbosity verbosity);

    /*
      Merge the two factors at index1 and index2.
    */
    int merge(
        int index1,
        int index2,
        utils::Verbosity verbosity);

    /*
      Extract the factor at the given index, rendering the FTS invalid.
    */
    std::pair<std::unique_ptr<MergeAndShrinkRepresentation>,
              std::unique_ptr<Distances>> extract_factor(int index);
    std::pair<std::unique_ptr<TransitionSystem>,
              std::unique_ptr<MergeAndShrinkRepresentation>>
        extract_ts_and_representation(int index);

    void statistics(int index) const;
    void dump(int index) const;
    void dump() const;

    const TransitionSystem &get_transition_system(int index) const {
        return *transition_systems[index];
    }

    const TransitionSystem *get_transition_system_raw_ptr(int index) const;
    const MergeAndShrinkRepresentation *get_mas_representation_raw_ptr(int index) const;

    const Distances &get_distances(int index) const {
        return *distances[index];
    }

    /*
      A factor is solvabe iff the distance of the initial state to some goal
      state is not infinity. Technically, the distance is infinity either if
      the information of Distances is infinity or if the initial state is
      pruned.
    */
    bool is_factor_solvable(int index) const;
    /*
      A factor is trivial iff all of its states are goal states and the
      corresponding merge-and-shrink representation is a total function.

      Notes:
      1) We require the merge-and-shrink representation to be a total function
      because otherwise, we would treat a factor as trivial even if it encodes
      information of dead ends. However, we do not consider the special case
      where pruning only pruned (unreachable) goal states, in which case the
      factor could arguably be considered trivial because such states are never
      encountered during search.

      2) As an alternative for requiring all states to be goal states, we
      considered requiring all represented variables to be non-goal variables.
      However, this would have the drawback to depend on information of the
      planning task and to be a syntactic rather than a semantic criterion,
      which counters the M&S spirit. It would also not treat a factor as
      trivial if it has only goal states due to shrinking, for example.
    */
    bool is_factor_trivial(int index) const;

    int get_num_active_entries() const {
        return num_active_entries;
    }

    // Used by LabelReduction and MergeScoringFunctionDFP
    const Labels &get_labels() const {
        return *labels;
    }

    // The following methods are used for iterating over the FTS
    FTSConstIterator begin() const {
        return FTSConstIterator(*this, false);
    }

    FTSConstIterator end() const {
        return FTSConstIterator(*this, true);
    }

    int get_size() const {
        return transition_systems.size();
    }

    bool is_active(int index) const;
};
}

#endif
