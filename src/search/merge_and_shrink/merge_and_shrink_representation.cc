#include "merge_and_shrink_representation.h"

#include "distances.h"
#include "types.h"

#include "../task_proxy.h"

#include "../utils/memory.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

using namespace std;

namespace merge_and_shrink {
MergeAndShrinkRepresentation::MergeAndShrinkRepresentation(int domain_size)
    : domain_size(domain_size) {
}

MergeAndShrinkRepresentation::~MergeAndShrinkRepresentation() {
}

int MergeAndShrinkRepresentation::get_domain_size() const {
    return domain_size;
}


MergeAndShrinkRepresentationLeaf::MergeAndShrinkRepresentationLeaf(
    int var_id, int domain_size)
    : MergeAndShrinkRepresentation(domain_size),
      var_id(var_id),
      lookup_table(domain_size) {
    iota(lookup_table.begin(), lookup_table.end(), 0);
}

MergeAndShrinkRepresentationLeaf::MergeAndShrinkRepresentationLeaf(const MergeAndShrinkRepresentationLeaf *other)
    : MergeAndShrinkRepresentation(other->domain_size),
      var_id(other->var_id),
      lookup_table(other->lookup_table) {
}

void MergeAndShrinkRepresentationLeaf::set_distances(
    const Distances &distances) {
    assert(distances.are_goal_distances_computed());
    for (int &entry : lookup_table) {
        if (entry != PRUNED_STATE) {
            entry = distances.get_goal_distance(entry);
        }
    }
}

void MergeAndShrinkRepresentationLeaf::apply_abstraction_to_lookup_table(
    const vector<int> &abstraction_mapping) {
    int new_domain_size = 0;
    for (int &entry : lookup_table) {
        if (entry != PRUNED_STATE) {
            entry = abstraction_mapping[entry];
            new_domain_size = max(new_domain_size, entry + 1);
        }
    }
    domain_size = new_domain_size;
}

int MergeAndShrinkRepresentationLeaf::get_value(const State &state) const {
    int value = state[var_id].get_value();
    return lookup_table[value];
}

bool MergeAndShrinkRepresentationLeaf::is_pruned() const {
    for (int entry : lookup_table) {
        if (entry == PRUNED_STATE) {
            return true;
        }
    }
    return false;
}

void MergeAndShrinkRepresentationLeaf::dump() const {
    cout << "lookup table (leaf): ";
    for (const auto &value : lookup_table) {
        cout << value << ", ";
    }
    cout << endl;
}


MergeAndShrinkRepresentationMerge::MergeAndShrinkRepresentationMerge(
    const std::shared_ptr<MergeAndShrinkRepresentation> &left_child_,
    const std::shared_ptr<MergeAndShrinkRepresentation> &right_child_)
    : MergeAndShrinkRepresentation(left_child_->get_domain_size() *
                                   right_child_->get_domain_size()),
      left_child(left_child_),
      right_child(right_child_),
      lookup_table(left_child->get_domain_size(),
                   vector<int>(right_child->get_domain_size())) {
    int counter = 0;
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            entry = counter;
            ++counter;
        }
    }
}

MergeAndShrinkRepresentationMerge::MergeAndShrinkRepresentationMerge(const MergeAndShrinkRepresentationMerge *other)
    : MergeAndShrinkRepresentation(other->domain_size),
      left_child(other->left_child),
      right_child(other->right_child),
      lookup_table(other->lookup_table) {
}

void MergeAndShrinkRepresentationMerge::set_distances(
    const Distances &distances) {
    assert(distances.are_goal_distances_computed());
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            if (entry != PRUNED_STATE) {
                entry = distances.get_goal_distance(entry);
            }
        }
    }
}

void MergeAndShrinkRepresentationMerge::apply_abstraction_to_lookup_table(
    const vector<int> &abstraction_mapping) {
    int new_domain_size = 0;
    for (vector<int> &row : lookup_table) {
        for (int &entry : row) {
            if (entry != PRUNED_STATE) {
                entry = abstraction_mapping[entry];
                new_domain_size = max(new_domain_size, entry + 1);
            }
        }
    }
    domain_size = new_domain_size;
}

int MergeAndShrinkRepresentationMerge::get_value(
    const State &state) const {
    int state1 = left_child->get_value(state);
    int state2 = right_child->get_value(state);
    if (state1 == PRUNED_STATE || state2 == PRUNED_STATE)
        return PRUNED_STATE;
    return lookup_table[state1][state2];
}

bool MergeAndShrinkRepresentationMerge::is_pruned() const {
    bool pruned1 = left_child->is_pruned();
    bool pruned2 = right_child->is_pruned();
    return pruned1 || pruned2;
}

void MergeAndShrinkRepresentationMerge::dump() const {
    cout << "lookup table (merge): " << endl;
    for (const auto &row : lookup_table) {
        for (const auto &value : row) {
            cout << value << ", ";
        }
        cout << endl;
    }
    cout << "left child:" << endl;
    left_child->dump();
    cout << "right child:" << endl;
    right_child->dump();
}
}
