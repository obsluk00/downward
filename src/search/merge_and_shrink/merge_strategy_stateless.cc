#include "merge_strategy_stateless.h"

#include "merge_selector.h"

#include "../plugins/plugin.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
MergeStrategyStateless::MergeStrategyStateless(
    const FactoredTransitionSystem &fts,
    const shared_ptr<MergeSelector> &merge_selector)
    : MergeStrategy(fts),
      merge_selector(merge_selector) {
}

NextMerge MergeStrategyStateless::get_next() {
    vector<pair<int, int>> merge_candidates = merge_selector->select_merge(fts);
    if (merge_candidates.size() > 1) {
        cerr << "More than one merge candidate remained after computing all "
                "scores! Did you forget to include a uniquely tie-breaking "
                "scoring function, e.g. total_order or single_random?" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    pair<int, int> next_pair = merge_candidates.front();
    return NextMerge(next_pair);
}
}
