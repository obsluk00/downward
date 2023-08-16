#include "order_generator_greedy.h"

#include "cost_partitioning.h"
#include "saturated_cost_partitioning_utils.h"
#include "transition_system.h"
#include "utils.h"

#include "../merge_and_shrink/types.h"
#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../utils/collections.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
OrderGeneratorGreedy::OrderGeneratorGreedy(const plugins::Options &opts)
    : OrderGenerator(opts),
      scoring_function(opts.get<ScoringFunction>("scoring_function")) {
}

double OrderGeneratorGreedy::rate_abstraction(
    const vector<int> &abstract_state_ids, int abs_id) const {
    assert(utils::in_bounds(abs_id, abstract_state_ids));
    int abstract_state_id = abstract_state_ids[abs_id];
    int h;
    if (abstract_state_id == merge_and_shrink::PRUNED_STATE) {
        h = INF;
    } else {
        assert(utils::in_bounds(abs_id, h_values_by_abstraction));
        assert(utils::in_bounds(abstract_state_id, h_values_by_abstraction[abs_id]));
        h = h_values_by_abstraction[abs_id][abstract_state_id];
        assert(h >= 0);
    }

    assert(utils::in_bounds(abs_id, stolen_costs_by_abstraction));
    int stolen_costs = stolen_costs_by_abstraction[abs_id];

    return compute_score(h, stolen_costs, scoring_function);
}

Order OrderGeneratorGreedy::compute_static_greedy_order_for_sample(
    const vector<int> &abstract_state_ids, utils::LogProxy &log) const {
    assert(abstract_state_ids.size() == h_values_by_abstraction.size());
    int num_abstractions = abstract_state_ids.size();
    Order order = get_default_order(num_abstractions);
    // Shuffle order to break ties randomly.
    rng->shuffle(order);
    vector<double> scores;
    scores.reserve(num_abstractions);
    for (int abs = 0; abs < num_abstractions; ++abs) {
        scores.push_back(rate_abstraction(abstract_state_ids, abs));
    }
    sort(order.begin(), order.end(), [&](int abs1, int abs2) {
             return scores[abs1] > scores[abs2];
         });
    if (log.is_at_least_verbose()) {
        log << "Static greedy scores: " << scores << endl;
        unordered_set<double> unique_scores(scores.begin(), scores.end());
        log << "Static greedy unique scores: " << unique_scores.size() << endl;
        log << "Static greedy order: " << order << endl;
    }
    return order;
}

void OrderGeneratorGreedy::precompute_info(
    const Abstractions &abstractions,
    const vector<int> &costs,
    utils::LogProxy &log) {
    assert(h_values_by_abstraction.empty());
    assert(stolen_costs_by_abstraction.empty());
    h_values_by_abstraction.reserve(abstractions.size());
    stolen_costs_by_abstraction.reserve(abstractions.size());

    vector<vector<int>> saturated_costs_by_abstraction;
    saturated_costs_by_abstraction.reserve(abstractions.size());
    for (const unique_ptr<Abstraction> &abstraction : abstractions) {
        vector<int> h_values = compute_goal_distances_for_abstraction(
            *abstraction, costs, log);
        vector<int> saturated_costs = compute_saturated_costs_for_abstraction(
            *abstraction, h_values, costs.size(), log);
        h_values_by_abstraction.push_back(move(h_values));
        saturated_costs_by_abstraction.push_back(move(saturated_costs));
    }

    vector<int> surplus_costs = compute_all_surplus_costs(
        costs, saturated_costs_by_abstraction);

    int num_abstractions = abstractions.size();
    for (int abs = 0; abs < num_abstractions; ++abs) {
        int sum_stolen_costs = compute_costs_stolen_by_heuristic(
            saturated_costs_by_abstraction[abs], surplus_costs);
        stolen_costs_by_abstraction.push_back(sum_stolen_costs);
    }
}

void OrderGeneratorGreedy::clear_internal_state() {
    vector<vector<int>>().swap(h_values_by_abstraction);
    vector<int>().swap(stolen_costs_by_abstraction);
}

Order OrderGeneratorGreedy::compute_order(
    const Abstractions &abstractions,
    const vector<int> &costs,
    utils::LogProxy &log,
    const vector<int> &abstract_state_ids) {
    if (h_values_by_abstraction.empty()) {
        precompute_info(abstractions, costs, log);
    } else {
        assert(h_values_by_abstraction.size() == abstractions.size());
        assert(stolen_costs_by_abstraction.size() == abstractions.size());
    }

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
        order = compute_static_greedy_order_for_sample(
            init_abstract_state_ids, log);
    } else {
        order = compute_static_greedy_order_for_sample(
            abstract_state_ids, log);
    }

    if (log.is_at_least_verbose()) {
        log << "Time for computing greedy order: " << greedy_timer << endl;
    }

    assert(order.size() == abstractions.size());
    return order;
}


class OrderGeneratorGreedyFeature : public plugins::TypedFeature<OrderGenerator, OrderGeneratorGreedy> {
public:
    OrderGeneratorGreedyFeature() : TypedFeature("greedy_orders") {
        add_scoring_function_option_to_feature(*this);
        add_common_order_generator_options(*this);
    }
};

static plugins::FeaturePlugin<OrderGeneratorGreedyFeature> _plugin;
}
