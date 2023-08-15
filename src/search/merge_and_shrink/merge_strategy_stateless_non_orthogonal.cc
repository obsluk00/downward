#include "merge_strategy_stateless_non_orthogonal.h"

#include "merge_selector.h"

#include "../plugins/plugin.h"
//TODO: use this perhaps
#include "../utils/rng.h"

#include <random>
#include <cassert>

using namespace std;

namespace merge_and_shrink {
MergeStrategyStatelessNonOrthogonal::MergeStrategyStatelessNonOrthogonal(
    const FactoredTransitionSystem &fts,
    const shared_ptr<MergeSelector> &merge_selector,
    int tokens)
    : MergeStrategy(fts),
      merge_selector(merge_selector),
      tokens(tokens) {
}

NextMerge MergeStrategyStatelessNonOrthogonal::get_next() {
    // if there are no merges stored, get new ones
    if(stored_merges.empty()) {
        stored_merges = merge_selector->select_merge(fts);
        // if there are more than one merge returned in this iterations, shuffle them, see how many overlap
        // and track how many times we would have to clone
        if (stored_merges.size() > 1) {
            random_device rd = random_device {};
            std::shuffle(stored_merges.begin(), stored_merges.end(), rd);
            var_count.clear();
            times_to_clone = 0;
            for (pair<int,int> merge : stored_merges) {
                var_count[merge.first] = 0;
                var_count[merge.second] = 0;
            }
            for (pair<int,int> merge : stored_merges) {
                var_count[merge.first] += 1;
                var_count[merge.second] += 1;
                if (var_count[merge.first] > 1)
                    times_to_clone += 1;
                if (var_count[merge.second] > 1)
                    times_to_clone += 1;
            }
        }
    }
    assert(!stored_merges.empty());
    // if we dont have enough tokens to clone we just tiebreak at random and delete the stored merges
    // TODO: consider other methods to handle insufficient tokens
    if (stored_merges.size() == 1 || times_to_clone > tokens) {
        NextMerge next_merge = NextMerge(stored_merges.back());
        stored_merges.clear();
        return next_merge;
    } else {
        NextMerge next_merge = NextMerge(stored_merges.back());
        stored_merges.pop_back();
        // check if we need to clone and decrement counters
        if (var_count[next_merge.indices.first] > 1) {
            var_count[next_merge.indices.first] -= 1;
            tokens -= 1;
            next_merge.clone.first = true;
            times_to_clone -= 1;
        }
        if (var_count[next_merge.indices.second] > 1) {
            var_count[next_merge.indices.second] -= 1;
            tokens -= 1;
            next_merge.clone.second = true;
            times_to_clone -= 1;

        }
        return next_merge;
    }
}
}
