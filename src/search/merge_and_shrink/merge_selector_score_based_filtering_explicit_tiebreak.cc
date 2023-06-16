#include "merge_selector_score_based_filtering_explicit_tiebreak.h"

#include "factored_transition_system.h"
#include "merge_scoring_function.h"

#include "../plugins/plugin.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
MergeSelectorScoreBasedFilteringExplicitTiebreak::MergeSelectorScoreBasedFilteringExplicitTiebreak(
    const plugins::Options &options)
    : merge_scoring_functions(
          options.get_list<shared_ptr<MergeScoringFunction>>(
              "scoring_functions")),
      tiebreaking_scoring_function(options.get<shared_ptr<MergeScoringFunction>>("tiebreaking_function"))        {
}

vector<pair<int, int>> MergeSelectorScoreBasedFilteringExplicitTiebreak::get_remaining_candidates(
    const vector<pair<int, int>> &merge_candidates,
    const vector<double> &scores) const {
    assert(merge_candidates.size() == scores.size());
    double best_score = INF;
    for (double score : scores) {
        if (score < best_score) {
            best_score = score;
        }
    }

    vector<pair<int, int>> result;
    for (size_t i = 0; i < scores.size(); ++i) {
        if (scores[i] == best_score) {
            result.push_back(merge_candidates[i]);
        }
    }
    return result;
}

pair<int, int> MergeSelectorScoreBasedFilteringExplicitTiebreak::select_merge(
    const FactoredTransitionSystem &fts,
    const vector<int> &indices_subset) const {
    vector<pair<int, int>> merge_candidates =
        compute_merge_candidates(fts, indices_subset);

    for (const shared_ptr<MergeScoringFunction> &scoring_function :
         merge_scoring_functions) {
        vector<double> scores = scoring_function->compute_scores(
            fts, merge_candidates);
        merge_candidates = get_remaining_candidates(merge_candidates, scores);
        if (merge_candidates.size() == 1) {
            break;
        }
    }

    if (merge_candidates.size() > 1) {
        bool clone_first = false;
        bool clone_second = false;
        // TODO: fix bug here
        for (pair<int, int> candidate : merge_candidates) {
            for (pair<int, int> other_candidate : merge_candidates) {
                if (other_candidate.first == candidate.first && other_candidate.second == candidate.second)
                    continue;
                if (!clone_first && (other_candidate.first == (candidate.first || (-1 * candidate.first)) || other_candidate.second == (candidate.first || (-1 * candidate.first)))) {
                    clone_first = true;
                    candidate.first = candidate.first * -1;
                }
                if (!clone_second && (other_candidate.first == (candidate.second || (-1 * candidate.second)) || other_candidate.second == (candidate.second || (-1 * candidate.second)))) {
                    clone_second = true;
                    candidate.second = candidate.second * -1;
                }
                if (clone_first && clone_second)
                    break;
            }
        }
        vector<double> scores = tiebreaking_scoring_function->compute_scores(
                fts, merge_candidates);
        merge_candidates = get_remaining_candidates(merge_candidates, scores);
    }

    return merge_candidates.front();
}

void MergeSelectorScoreBasedFilteringExplicitTiebreak::initialize(const TaskProxy &task_proxy) {
    for (shared_ptr<MergeScoringFunction> &scoring_function
         : merge_scoring_functions) {
        scoring_function->initialize(task_proxy);
    }
}

string MergeSelectorScoreBasedFilteringExplicitTiebreak::name() const {
    return "score based filtering";
}

void MergeSelectorScoreBasedFilteringExplicitTiebreak::dump_selector_specific_options(
    utils::LogProxy &log) const {
    if (log.is_at_least_normal()) {
        for (const shared_ptr<MergeScoringFunction> &scoring_function
             : merge_scoring_functions) {
            scoring_function->dump_options(log);
        }
    }
}

bool MergeSelectorScoreBasedFilteringExplicitTiebreak::requires_init_distances() const {
    for (const shared_ptr<MergeScoringFunction> &scoring_function
         : merge_scoring_functions) {
        if (scoring_function->requires_init_distances()) {
            return true;
        }
    }
    return false;
}

bool MergeSelectorScoreBasedFilteringExplicitTiebreak::requires_goal_distances() const {
    for (const shared_ptr<MergeScoringFunction> &scoring_function
         : merge_scoring_functions) {
        if (scoring_function->requires_goal_distances()) {
            return true;
        }
    }
    return false;
}

class MergeSelectorScoreBasedFilteringExplicitTiebreakFeature : public plugins::TypedFeature<MergeSelector, MergeSelectorScoreBasedFilteringExplicitTiebreak> {
public:
    MergeSelectorScoreBasedFilteringExplicitTiebreakFeature() : TypedFeature("score_based_filtering_explicit_tiebreak") {
        document_title("Score based filtering merge selector with explicit tiebreaking");
        document_synopsis(
            "This merge selector has a list of scoring functions, which are used "
            "iteratively to compute scores for merge candidates, keeping the best "
            "ones (with minimal scores). If more than one are left, the chosen tiebreaking function decides which merge will be recommended.");

        add_list_option<shared_ptr<MergeScoringFunction>>(
            "scoring_functions",
            "The list of scoring functions used to compute scores for candidates.");
        add_option<shared_ptr<MergeScoringFunction>>(
                "tiebreaking_function",
                "The scoring function used to tiebreak if multiple candidates are equally good.");
    }
};

static plugins::FeaturePlugin<MergeSelectorScoreBasedFilteringExplicitTiebreakFeature> _plugin;
}
