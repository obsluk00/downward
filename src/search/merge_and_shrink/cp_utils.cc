#include "cp_utils.h"

#include "cost_partitioning.h"
#include "factored_transition_system.h"
#include "labels.h"
#include "merge_and_shrink_representation.h"

#include "../utils/memory.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
vector<int> compute_label_costs(
    const Labels &labels) {
    int num_labels = labels.get_num_total_labels();
    vector<int> label_costs(num_labels, -1);
    for (int label_no : labels) {
        label_costs[label_no] = labels.get_label_cost(label_no);
    }
    return label_costs;
}

vector<unique_ptr<Abstraction>> compute_abstractions_for_factors(
    const FactoredTransitionSystem &fts,
    const vector<int> &considered_factors) {
    vector<unique_ptr<Abstraction>> abstractions;
    abstractions.reserve(considered_factors.size());
    for (int index : considered_factors) {
        assert(fts.is_active(index));
        const TransitionSystem *transition_system = fts.get_transition_system_raw_ptr(index);
        unique_ptr<MergeAndShrinkRepresentation> mas_representation = nullptr;
        if (dynamic_cast<const MergeAndShrinkRepresentationLeaf *>(fts.get_mas_representation_raw_ptr(index))) {
            mas_representation = utils::make_unique_ptr<MergeAndShrinkRepresentationLeaf>(
                dynamic_cast<const MergeAndShrinkRepresentationLeaf *>
                (fts.get_mas_representation_raw_ptr(index)));
        } else {
            mas_representation = utils::make_unique_ptr<MergeAndShrinkRepresentationMerge>(
                dynamic_cast<const MergeAndShrinkRepresentationMerge *>(
                    fts.get_mas_representation_raw_ptr(index)));
        }
        abstractions.push_back(utils::make_unique_ptr<Abstraction>(transition_system, move(mas_representation)));
    }
    return abstractions;
}
}

