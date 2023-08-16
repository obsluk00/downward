#include "merge_scoring_function_cp.h"

#include "cp_utils.h"
#include "cost_partitioning.h"
#include "distances.h"
#include "factored_transition_system.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "saturated_cost_partitioning.h"
#include "shrink_strategy.h"
#include "transition_system.h"
#include "merge_scoring_function_miasm_utils.h"

#include "../task_proxy.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"

#include "../tasks/root_task.h"

#include "../utils/logging.h"
#include "../utils/markup.h"

using namespace std;

namespace merge_and_shrink {
MergeScoringFunctionCP::MergeScoringFunctionCP(
    const plugins::Options &options)
    : shrink_strategy(options.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
      max_states(options.get<int>("max_states")),
      max_states_before_merge(options.get<int>("max_states_before_merge")),
      shrink_threshold_before_merge(options.get<int>("threshold_before_merge")),
      cp_factory(options.get<shared_ptr<CostPartitioningFactory>>("cost_partitioning")),
      filter_trivial_factors(options.get<bool>("filter_trivial_factors")) {
}

vector<double> MergeScoringFunctionCP::compute_scores(
    const FactoredTransitionSystem &fts,
    const vector<pair<int, int>> &merge_candidates) {
    /*
      Score: h^CP(ts1, ts2) - h^prod(init)
      From CP(ts1, ts2) <= CP(prod), this difference is never larger than 0.
      If it is 0, "merging is not useful" because CP already captures the same
      information. Otherwise, the lower it is, the better it is to compute the
      product instead of leaving it to the CP because the CP is not good on the
      product.
    */
    vector<double> scores;
    scores.reserve(merge_candidates.size());
    vector<int> trivial_factors(fts.get_size(), -1);
    for (pair<int, int> merge_candidate : merge_candidates) {
        int index1 = merge_candidate.first;
        int index2 = merge_candidate.second;
        if (filter_trivial_factors) {
            if (trivial_factors[index1] == -1) {
                trivial_factors[index1] = fts.is_factor_trivial(index1);
            }
            if (trivial_factors[index2] == -1) {
                trivial_factors[index2] = fts.is_factor_trivial(index2);
            }
            if (trivial_factors[index1] == 1 || trivial_factors[index2] == 1) {
                // Trivial abstraction do not contribute to SCP, therefore
                // no improvement over the previous best heuristic value.
                scores.push_back(0);
                continue;
            }
        }

        utils::LogProxy log = utils::get_silent_log();
        // Compute initial h value of the product.
        unique_ptr<TransitionSystem> product = shrink_before_merge_externally(
            fts,
            index1,
            index2,
            *shrink_strategy,
            max_states,
            max_states_before_merge,
            shrink_threshold_before_merge,
            log);
        unique_ptr<Distances> distances = utils::make_unique_ptr<Distances>(*product);
        const bool compute_init_distances = true;
        const bool compute_goal_distances = true;
        distances->compute_distances(compute_init_distances, compute_goal_distances, log);
        int product_init_h = distances->get_goal_distance(product->get_init_state());


        // TODO: this is a hack that we could avoid by being able to have a
        // cost partitioning that works for abstract states rather than
        // concrete states. By doing so we could actually avoid copying mas
        // representations.
        State init_state(*tasks::g_root_task, tasks::g_root_task->get_initial_state_values());
        int cp_init_h = -1;

        // Compute the init h-value of the CP(s) over the product.
        if (dynamic_cast<SaturatedCostPartitioningFactory *>(cp_factory.get())) {
            // Compute SCPs for both orders.
            SaturatedCostPartitioningFactory *scp_factory = dynamic_cast<SaturatedCostPartitioningFactory *>(cp_factory.get());
            unique_ptr<CostPartitioning> cp1 = scp_factory->generate_for_order(
                compute_label_costs(fts.get_labels()),
                compute_abstractions_for_factors(fts, {index1, index2}),
                {0, 1},
                log);
            unique_ptr<CostPartitioning> cp2 = scp_factory->generate_for_order(
                compute_label_costs(fts.get_labels()),
                compute_abstractions_for_factors(fts, {index1, index2}),
                {1, 0},
                log);
            cp_init_h = max(cp1->compute_value(init_state), cp2->compute_value(init_state));
        } else {
            // Compute OCP.
            unique_ptr<CostPartitioning> cp = cp_factory->generate(
                compute_label_costs(fts.get_labels()),
                compute_abstractions_for_factors(fts, {index1, index2}),
                log);
            cp_init_h = cp->compute_value(init_state);
        }

        double score = cp_init_h - product_init_h;
        assert(score <= 0);
        scores.push_back(score);
    }
    return scores;
}

string MergeScoringFunctionCP::name() const {
    return "sf_cp";
}

class MergeScoringFunctionCPFeature : public plugins::TypedFeature<MergeScoringFunction, MergeScoringFunctionCP> {
public:
    MergeScoringFunctionCPFeature() : TypedFeature("sf_cp") {
        // TODO: use shrink strategy and limit options from MergeAndShrinkHeuristic
        // instead of having the identical options here again.
        add_option<shared_ptr<ShrinkStrategy>>(
            "shrink_strategy",
            "We recommend setting this to match the shrink strategy configuration "
            "given to {{{merge_and_shrink}}}, see note below.");
        add_transition_system_size_limit_options_to_feature(*this);

        add_option<shared_ptr<CostPartitioningFactory>>(
            "cost_partitioning",
            "A method for computing cost partitionings over intermediate "
            "'snapshots' of the factored transition system.");
        add_option<bool>(
            "filter_trivial_factors",
            "If true, do not consider trivial factors for computing CPs. Should "
            "be set to true when computing SCPs.");
    }

    virtual shared_ptr<MergeScoringFunctionCP> create_component(
        const plugins::Options &options, const utils::Context &context) const override {
        plugins::Options options_copy(options);
        handle_shrink_limit_options_defaults(options_copy, context);
        return make_shared<MergeScoringFunctionCP>(options_copy);
    }
};

static plugins::FeaturePlugin<MergeScoringFunctionCPFeature> _plugin;
}
