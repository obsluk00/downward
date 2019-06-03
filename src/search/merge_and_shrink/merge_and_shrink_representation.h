#ifndef MERGE_AND_SHRINK_MERGE_AND_SHRINK_REPRESENTATION_H
#define MERGE_AND_SHRINK_MERGE_AND_SHRINK_REPRESENTATION_H

#include <memory>
#include <vector>

class State;

namespace merge_and_shrink {
class Distances;
class MergeAndShrinkRepresentation {
protected:
    int domain_size;

public:
    explicit MergeAndShrinkRepresentation(int domain_size);
    virtual ~MergeAndShrinkRepresentation() = 0;

    int get_domain_size() const;

    // Store distances instead of abstract state numbers.
    virtual void set_distances(const Distances &) = 0;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) = 0;
    /* Return the abstract state or the goal distance, depending on whether
      set_distances has been used or not. */
    virtual int get_value(const State &state) const = 0;
    /* Check if this or any component merge-and-shrink representation contains
       PRUNED_STATE. */
    virtual bool is_pruned() const = 0;
    virtual void dump() const = 0;
};


class MergeAndShrinkRepresentationLeaf : public MergeAndShrinkRepresentation {
    const int var_id;

    std::vector<int> lookup_table;
public:
    MergeAndShrinkRepresentationLeaf(int var_id, int domain_size);
    explicit MergeAndShrinkRepresentationLeaf(const MergeAndShrinkRepresentationLeaf *other);
    virtual ~MergeAndShrinkRepresentationLeaf() = default;

    virtual void set_distances(const Distances &) override;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) override;
    virtual int get_value(const State &state) const override;
    virtual bool is_pruned() const override;
    virtual void dump() const override;
};


class MergeAndShrinkRepresentationMerge : public MergeAndShrinkRepresentation {
    std::shared_ptr<MergeAndShrinkRepresentation> left_child;
    std::shared_ptr<MergeAndShrinkRepresentation> right_child;
    std::vector<std::vector<int>> lookup_table;
public:
    MergeAndShrinkRepresentationMerge(
        const std::shared_ptr<MergeAndShrinkRepresentation> &left_child,
        const std::shared_ptr<MergeAndShrinkRepresentation> &right_child);
    explicit MergeAndShrinkRepresentationMerge(const MergeAndShrinkRepresentationMerge *other);
    virtual ~MergeAndShrinkRepresentationMerge() = default;

    virtual void set_distances(const Distances &distances) override;
    virtual void apply_abstraction_to_lookup_table(
        const std::vector<int> &abstraction_mapping) override;
    virtual int get_value(const State &state) const override;
    virtual bool is_pruned() const override;
    virtual void dump() const override;
};
}

#endif
