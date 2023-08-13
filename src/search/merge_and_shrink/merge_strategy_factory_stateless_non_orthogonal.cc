#include "merge_strategy_factory_stateless_non_orthogonal.h"

#include "merge_selector.h"
#include "merge_strategy_stateless_non_orthogonal.h"

#include "../plugins/plugin.h"
#include "../utils/memory.h"

using namespace std;

namespace merge_and_shrink {
MergeStrategyFactoryStatelessNonOrthogonal::MergeStrategyFactoryStatelessNonOrthogonal(
    const plugins::Options &options)
    : MergeStrategyFactory(options),
      merge_selector(options.get<shared_ptr<MergeSelector>>("merge_selector")),
      tokens(options.get<int>("tokens")){
}

unique_ptr<MergeStrategy> MergeStrategyFactoryStatelessNonOrthogonal::compute_merge_strategy(
    const TaskProxy &task_proxy,
    const FactoredTransitionSystem &fts) {
    merge_selector->initialize(task_proxy);
    return utils::make_unique_ptr<MergeStrategyStatelessNonOrthogonal>(fts, merge_selector, tokens);
}

string MergeStrategyFactoryStatelessNonOrthogonal::name() const {
    return "stateless non-orthogonal";
}

void MergeStrategyFactoryStatelessNonOrthogonal::dump_strategy_specific_options() const {
    if (log.is_at_least_normal()) {
        merge_selector->dump_options(log);
    }
}

bool MergeStrategyFactoryStatelessNonOrthogonal::requires_init_distances() const {
    return merge_selector->requires_init_distances();
}

bool MergeStrategyFactoryStatelessNonOrthogonal::requires_goal_distances() const {
    return merge_selector->requires_goal_distances();
}

class MergeStrategyFactoryStatelessNonOrthogonalFeature : public plugins::TypedFeature<MergeStrategyFactory, MergeStrategyFactoryStatelessNonOrthogonal> {
public:
    MergeStrategyFactoryStatelessNonOrthogonalFeature() : TypedFeature("merge_stateless_non_orthogonal") {
        document_title("Non-orthogonal stateless merge strategy");
        document_synopsis(
            "This merge strategy has a merge selector, which computes the next "
            "merges only depending on the current state of the factored transition "
            "system, not requiring any additional information. If sufficient tokens for cloning are available, "
            "all merges are performed.");

        add_option<int>(
                "tokens",
                "Amount of times the algorithm is allowed to cloned.");
        add_merge_strategy_options_to_feature(*this);

        add_option<shared_ptr<MergeSelector>>(
            "merge_selector",
            "The merge selector to be used.");
        add_merge_strategy_options_to_feature(*this);

        document_note(
            "Note",
            "Examples include the DFP merge strategy, which can be obtained using:\n"
            "{{{\n"
            "merge_strategy=merge_stateless(merge_selector=score_based_filtering("
            "scoring_functions=[goal_relevance,dfp,total_order(<order_option>))]))"
            "\n}}}\n"
            "and the (dynamic/score-based) MIASM strategy, which can be obtained "
            "using:\n"
            "{{{\n"
            "merge_strategy=merge_stateless(merge_selector=score_based_filtering("
            "scoring_functions=[sf_miasm(<shrinking_options>),total_order(<order_option>)]"
            "\n}}}");
    }
};

static plugins::FeaturePlugin<MergeStrategyFactoryStatelessNonOrthogonalFeature> _plugin;
}
