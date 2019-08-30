#include "labels.h"

#include "types.h"

#include "../utils/collections.h"
#include "../utils/logging.h"
#include "../utils/memory.h"

#include <cassert>
#include <iostream>
#include <numeric>

using namespace std;

namespace merge_and_shrink {
Labels::Labels(vector<unique_ptr<Label>> &&labels)
    : labels(move(labels)),
      max_size(0) {
    if (!this->labels.empty()) {
        max_size = this->labels.size() * 2 - 1;
    }
    original_to_current_labels.resize(this->labels.size());
    iota(original_to_current_labels.begin(), original_to_current_labels.end(), 0);
    for (int label_no = 0; label_no < static_cast<int>(this->labels.size()); ++label_no) {
        reduced_to_original_labels[label_no] = {label_no};
    }
}

void Labels::reduce_labels(const vector<int> &old_label_nos) {
    /*
      Even though we currently only support exact label reductions where
      reduced labels are of equal cost, to support non-exact label reductions,
      we compute the cost of the new label as the minimum cost of all old
      labels reduced to it to satisfy admissibility.
    */
    int new_label_cost = INF;
    for (size_t i = 0; i < old_label_nos.size(); ++i) {
        int old_label_no = old_label_nos[i];
        int cost = get_label_cost(old_label_no);
        if (cost < new_label_cost) {
            new_label_cost = cost;
        }
        labels[old_label_no] = nullptr;
    }
    int new_label_no = labels.size();
    labels.push_back(utils::make_unique_ptr<Label>(new_label_cost));
    vector<int> new_original_labels;
    for (int old_label_no : old_label_nos) {
        for (size_t orig_label_no = 0; orig_label_no < original_to_current_labels.size(); ++orig_label_no) {
            if (original_to_current_labels[orig_label_no] == old_label_no) {
                original_to_current_labels[orig_label_no] = new_label_no;
            }
        }
        // Keep the mapping for all intermediate reduced labels alive.
        const vector<int> &original_labels = reduced_to_original_labels[old_label_no];
        new_original_labels.insert(
            new_original_labels.end(),
            original_labels.begin(),
            original_labels.end());
    }
    reduced_to_original_labels[new_label_no] = move(new_original_labels);

//    for (size_t label_no = 0; label_no < original_to_current_labels.size(); ++label_no) {
//        int abs_label = original_to_current_labels[label_no];
//        const vector<int> &orig_labels = reduced_to_original_labels.at(abs_label);
//        cout << "label " << label_no << " is mapped to abs label "
//             << abs_label << " which has original labels: " << orig_labels << endl;
//        assert(find(orig_labels.begin(), orig_labels.end(), label_no) != orig_labels.end());
//        for (int orig_label : orig_labels) {
//            assert(original_to_current_labels[orig_label] == abs_label);
//        }
//    }
}

bool Labels::is_current_label(int label_no) const {
    assert(utils::in_bounds(label_no, labels));
    return labels[label_no] != nullptr;
}

int Labels::get_label_cost(int label_no) const {
    assert(labels[label_no]);
    return labels[label_no]->get_cost();
}

void Labels::dump_labels() const {
    cout << "active labels:" << endl;
    for (size_t label_no = 0; label_no < labels.size(); ++label_no) {
        if (labels[label_no]) {
            cout << "label " << label_no
                 << ", cost " << labels[label_no]->get_cost()
                 << endl;
        }
    }
}
}
