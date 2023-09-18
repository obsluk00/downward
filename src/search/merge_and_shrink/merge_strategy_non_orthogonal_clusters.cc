#include "merge_strategy_non_orthogonal_clusters.h"

#include "factored_transition_system.h"
#include "merge_selector.h"
#include "merge_tree.h"
#include "merge_tree_factory.h"
#include "transition_system.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {
MergeStrategyNonOrthogonalClusters::MergeStrategyNonOrthogonalClusters (
    const FactoredTransitionSystem &fts,
    const TaskProxy &task_proxy,
    const shared_ptr<MergeSelector> &merge_selector,
    vector<vector<int>> clusters,
    map<int,int> var_count,
    int tokens)
    : MergeStrategy(fts),
      task_proxy(task_proxy),
      merge_selector(merge_selector),
      clusters(move(clusters)),
      var_count(move(var_count)),
      tokens(tokens) {
}

MergeStrategyNonOrthogonalClusters::~MergeStrategyNonOrthogonalClusters() {
}

//TODO: change documentation
NextMerge MergeStrategyNonOrthogonalClusters::get_next() {
    // We did not already start merging an SCC/all finished SCCs, so we
    // do not have a current set of indices we want to finish merging.
    if (current_ts_indices.empty()) {
        // Get the next indices we need to merge
        if (clusters.empty()) {
            // TODO: deal with this more elegantly
            return NextMerge({-1, -1}, true);
        } else {
            vector<int> &current_cluster = clusters.front();
            assert(current_cluster.size() > 1);
            current_ts_indices = move(current_cluster);
            clusters.erase(clusters.begin());
        }
    } else {
        // Add the most recent merge to the current indices set
        current_ts_indices.push_back(fts.get_size() - 1);
    }
    // Select the next merge for the current set of indices, using the selector.
    vector<pair<int, int>>next_pairs = merge_selector->select_merge(fts, current_ts_indices);
    assert(next_pairs.size() == 1);
    pair<int, int> next_pair = next_pairs.front();

    // Remove the two merged indices from the current set of indices.
    for (vector<int>::iterator it = current_ts_indices.begin();
         it != current_ts_indices.end();) {
        if (*it == next_pair.first || *it == next_pair.second) {
            it = current_ts_indices.erase(it);
        } else {
            ++it;
        }
    }

    pair<bool, bool> clone = {false, false};
    if (var_count[next_pair.first] > 0) {
        var_count[next_pair.first] -= 1;
        clone.first = true;
    }
    if (var_count[next_pair.second] > 0) {
        var_count[next_pair.second] -= 1;
        clone.second = true;
    }
    return NextMerge(next_pair, false, clone);
}
}
