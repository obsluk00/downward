#include "merge_scoring_function_avoid_existing.h"

#include "factored_transition_system.h"
#include "transition_system.h"
#include "utils.h"

#include "../plugins/plugin.h"

using namespace std;

namespace merge_and_shrink {
    vector<double> MergeScoringFunctionAvoidExisting::compute_scores(
            const FactoredTransitionSystem &fts,
            const vector<pair<int, int>> &merge_candidates) {

        vector<double> scores;
        scores.reserve(merge_candidates.size());
        for (pair<int, int> merge_candidate : merge_candidates) {
            int ts_index1 = merge_candidate.first;
            int ts_index2 = merge_candidate.second;
            int score = 0;

            // extract variable sets and create union of what would be the merged system
            vector<int> variables_index1 = fts.get_transition_system(ts_index1).get_incorporated_variables();
            vector<int> variables_index2 = fts.get_transition_system(ts_index2).get_incorporated_variables();
            vector<int> variables_merge;
            set_union(variables_index1.cbegin(), variables_index1.cend(),
                      variables_index2.cbegin(), variables_index2.cend(),
                      back_inserter(variables_merge));

            for (int ts_index : fts) {
                const TransitionSystem &ts = fts.get_transition_system(ts_index);
                if (ts.get_incorporated_variables() == variables_merge) {
                    score = INF;
                }
            }
            scores.push_back(score);
        }
        return scores;
    }

    string MergeScoringFunctionAvoidExisting::name() const {
        return "avoid existing";
    }

    class MergeScoringFunctionAvoidExistingFeature : public plugins::TypedFeature<MergeScoringFunction, MergeScoringFunctionAvoidExisting> {
    public:
        MergeScoringFunctionAvoidExistingFeature() : TypedFeature("avoid_existing") {
            document_title( "Avoid Existing");
            document_synopsis(
                    "This scoring function assigns a merge candidate a value of 0 iff the "
                    "union of the variable sets of their factors is not identical to the "
                    "variable set of any factor in the factored transition system. "
                    "All other candidates get a score of positive infinity.");
        }

        virtual shared_ptr<MergeScoringFunctionAvoidExisting> create_component(const plugins::Options &, const utils::Context &) const override {
            return make_shared<MergeScoringFunctionAvoidExisting>();
        }
    };

    static plugins::FeaturePlugin<MergeScoringFunctionAvoidExistingFeature> _plugin;
}
