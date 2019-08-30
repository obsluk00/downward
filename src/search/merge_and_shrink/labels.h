#ifndef MERGE_AND_SHRINK_LABELS_H
#define MERGE_AND_SHRINK_LABELS_H

#include <memory>
#include <unordered_map>
#include <vector>

namespace merge_and_shrink {
class Label {
    /*
      This class implements labels as used by merge-and-shrink transition systems.
      Labels are opaque tokens that have an associated cost.
    */
    int cost;
public:
    explicit Label(int cost_)
        : cost(cost_) {
    }
    ~Label() {}
    int get_cost() const {
        return cost;
    }
};

/*
  This class serves both as a container class to handle the set of all labels
  and to perform label reduction on this set.
*/
class Labels {
    std::vector<std::unique_ptr<Label>> labels;
    int max_size; // the maximum number of labels that can be created
    std::vector<int> original_to_current_labels;
    std::unordered_map<int, std::vector<int>> reduced_to_original_labels;
public:
    explicit Labels(std::vector<std::unique_ptr<Label>> &&labels);
    void reduce_labels(const std::vector<int> &old_label_nos);
    bool is_current_label(int label_no) const;
    int get_label_cost(int label_no) const;
    void dump_labels() const;
    int get_size() const {
        return labels.size();
    }
    int get_max_size() const {
        return max_size;
    }
    const std::vector<int> &get_original_to_current_labels() const {
        return original_to_current_labels;
    }
    const std::unordered_map<int, std::vector<int>> &get_reduced_to_original_labels() const {
        return reduced_to_original_labels;
    }
};
}

#endif
