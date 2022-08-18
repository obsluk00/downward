#include "merge_scoring_function_cp.h"

#include "cost_partitioning.h"
#include "distances.h"
#include "factored_transition_system.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "shrink_strategy.h"
#include "transition_system.h"
#include "merge_scoring_function_miasm_utils.h"

#include "../task_proxy.h"

#include "../options/option_parser.h"
#include "../options/options.h"
#include "../options/plugin.h"

#include "../tasks/root_task.h"

#include "../utils/logging.h"
#include "../utils/markup.h"

using namespace std;

namespace merge_and_shrink {
MergeScoringFunctionCP::MergeScoringFunctionCP(
    const options::Options &options)
    : shrink_strategy(options.get<shared_ptr<ShrinkStrategy>>("shrink_strategy")),
      max_states(options.get<int>("max_states")),
      max_states_before_merge(options.get<int>("max_states_before_merge")),
      shrink_threshold_before_merge(options.get<int>("threshold_before_merge")),
      cp_factory(options.get<shared_ptr<CostPartitioningFactory>>("cost_partitioning")),
      filter_trivial_factors(options.get<bool>("filter_trivial_factors")) {
}

vector<int> compute_label_costs(
    const Labels &labels) {
    int num_labels = labels.get_size();
    vector<int> label_costs(num_labels, -1);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels.is_current_label(label_no)) {
            label_costs[label_no] = labels.get_label_cost(label_no);
        }
    }
    return label_costs;
}

vector<unique_ptr<Abstraction>> MergeScoringFunctionCP::compute_abstractions_over_fts(
    const FactoredTransitionSystem &fts,
    const vector<int> &considered_factors) const {
    vector<unique_ptr<Abstraction>> abstractions;
    abstractions.reserve(considered_factors.size());
    for (int index : considered_factors) {
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

        // Compute the CP over the product.
        unique_ptr<CostPartitioning> cp = cp_factory->generate(
            compute_label_costs(fts.get_labels()),
            compute_abstractions_over_fts(fts, {index1, index2}),
            log);
        // TODO: this is a hack that we could avoid by being able to have a
        // cost partitioning that works for abstract states rather than
        // concrete states. By doing so we could actually avoid copying mas
        // representations.
        int cp_init_h = cp->compute_value(
            State(*tasks::g_root_task, tasks::g_root_task->get_initial_state_values()));

        double score = cp_init_h - product_init_h;
        assert(score <= 0);
        scores.push_back(score);
    }
    return scores;
}

string MergeScoringFunctionCP::name() const {
    return "sf_cp";
}

static shared_ptr<MergeScoringFunction>_parse(options::OptionParser &parser) {
    // TODO: use shrink strategy and limit options from MergeAndShrinkHeuristic
    // instead of having the identical options here again.
    parser.add_option<shared_ptr<ShrinkStrategy>>(
        "shrink_strategy",
        "We recommend setting this to match the shrink strategy configuration "
        "given to {{{merge_and_shrink}}}, see note below.");
    add_transition_system_size_limit_options_to_parser(parser);

    parser.add_option<shared_ptr<CostPartitioningFactory>>(
        "cost_partitioning",
        "A method for computing cost partitionings over intermediate "
        "'snapshots' of the factored transition system.");
    parser.add_option<bool>(
        "filter_trivial_factors",
        "If true, do not consider trivial factors for computing CPs. Should "
        "be set to true when computing SCPs.");

    options::Options options = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    handle_shrink_limit_options_defaults(options);

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return make_shared<MergeScoringFunctionCP>(options);
    }
}

static options::Plugin<MergeScoringFunction> _plugin("sf_cp", _parse);
}
