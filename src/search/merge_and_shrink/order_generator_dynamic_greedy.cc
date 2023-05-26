#include "order_generator_dynamic_greedy.h"

#include "cost_partitioning.h"
#include "saturated_cost_partitioning_utils.h"
#include "transition_system.h"
#include "utils.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"

#include "../utils/collections.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
OrderGeneratorDynamicGreedy::OrderGeneratorDynamicGreedy(const plugins::Options &opts)
    : OrderGenerator(opts),
      scoring_function(opts.get<ScoringFunction>("scoring_function")) {
}

Order OrderGeneratorDynamicGreedy::compute_dynamic_greedy_order_for_sample(
    const Abstractions &abstractions,
    const vector<int> &abstract_state_ids,
    vector<int> remaining_costs) const {
    assert(abstractions.size() == abstract_state_ids.size());
    vector<int> remaining_abstractions = get_default_order(abstractions.size());

    Order order;
    utils::LogProxy log = utils::get_silent_log();
    while (!remaining_abstractions.empty()) {
        int num_remaining = remaining_abstractions.size();
        vector<int> current_h_values;
        vector<vector<int>> current_saturated_costs;
        current_h_values.reserve(num_remaining);
        current_saturated_costs.reserve(num_remaining);

        // Shuffle remaining abstractions to break ties randomly.
        rng->shuffle(remaining_abstractions);
        vector<int> saturated_costs_for_best_abstraction;
        for (int abs_id : remaining_abstractions) {
            assert(utils::in_bounds(abs_id, abstract_state_ids));
            int abstract_state_id = abstract_state_ids[abs_id];
            Abstraction &abstraction = *abstractions[abs_id];
            vector<int> h_values = compute_goal_distances_for_abstraction(
                abstraction, remaining_costs, log);
            vector<int> saturated_costs = compute_saturated_costs_for_abstraction(
                abstraction, h_values, remaining_costs.size(), log);
            assert(utils::in_bounds(abstract_state_id, h_values));
            int h = h_values[abstract_state_id];
            current_h_values.push_back(h);
            current_saturated_costs.push_back(move(saturated_costs));
        }

        vector<int> surplus_costs = compute_all_surplus_costs(
            remaining_costs, current_saturated_costs);

        double highest_score = -numeric_limits<double>::max();
        int best_rem_id = -1;
        for (int rem_id = 0; rem_id < num_remaining; ++rem_id) {
            int used_costs = compute_costs_stolen_by_heuristic(
                current_saturated_costs[rem_id], surplus_costs);
            double score = compute_score(
                current_h_values[rem_id], used_costs, scoring_function);
            if (score > highest_score) {
                best_rem_id = rem_id;
                highest_score = score;
            }
        }
        assert(utils::in_bounds(best_rem_id, remaining_abstractions));
        order.push_back(remaining_abstractions[best_rem_id]);
        reduce_costs(remaining_costs, current_saturated_costs[best_rem_id]);
        utils::swap_and_pop_from_vector(remaining_abstractions, best_rem_id);
    }
    return order;
}

Order OrderGeneratorDynamicGreedy::compute_order(
    const Abstractions &abstractions,
    const vector<int> &costs,
    utils::LogProxy &log,
    const vector<int> &abstract_state_ids) {
    utils::Timer greedy_timer;

    vector<int> order;
    if (abstract_state_ids.empty()) {
        if (log.is_at_least_verbose()) {
            log << "No sample given; use initial state." << endl;
        }
        vector<int> init_abstract_state_ids;
        init_abstract_state_ids.reserve(abstractions.size());
        for (const auto &abstraction : abstractions) {
            init_abstract_state_ids.push_back(abstraction->transition_system->get_init_state());
        }
        order = compute_dynamic_greedy_order_for_sample(
            abstractions, init_abstract_state_ids, costs);
    } else {
        order = compute_dynamic_greedy_order_for_sample(
            abstractions, abstract_state_ids, costs);
    }

    if (log.is_at_least_verbose()) {
        log << "Time for computing dynamic greedy order: "
            << greedy_timer << endl;
    }

    assert(order.size() == abstractions.size());
    return order;
}

class OrderGeneratorDynamicGreedyFeature : public plugins::TypedFeature<OrderGenerator, OrderGeneratorDynamicGreedy> {
public:
    OrderGeneratorDynamicGreedyFeature() : TypedFeature("dynamic_greedy_orders") {
        add_scoring_function_option_to_feature(*this);
        add_common_order_generator_options(*this);
    }
};

static plugins::FeaturePlugin<OrderGeneratorDynamicGreedyFeature> _plugin;
}
