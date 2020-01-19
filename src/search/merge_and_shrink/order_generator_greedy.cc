#include "order_generator_greedy.h"

#include "cost_partitioning.h"
#include "transition_system.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../merge_and_shrink/types.h"

#include "../utils/collections.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
MASOrderGeneratorGreedy::MASOrderGeneratorGreedy(const Options &opts)
    : MASOrderGenerator(opts),
      scoring_function(
          static_cast<ScoringFunction>(opts.get_enum("scoring_function"))) {
}

double MASOrderGeneratorGreedy::rate_abstraction(
    const unique_ptr<Abstraction> &abs, const vector<int> &h_values, int stolen_costs) const {

    int abstract_state_id = abs->transition_system->get_init_state();
    int h;
    if (abstract_state_id == merge_and_shrink::PRUNED_STATE) {
        h = INF;
    } else {
        assert(utils::in_bounds(abstract_state_id, h_values));
        h = h_values[abstract_state_id];
        assert(h >= 0);
    }

    return compute_score(h, stolen_costs, scoring_function);
}

void MASOrderGeneratorGreedy::initialize(const TaskProxy &) {
}

Order MASOrderGeneratorGreedy::compute_order_for_state(
    const Abstractions &abstractions,
    const vector<int> &costs,
    const std::vector<std::vector<int>> &h_values_by_abstraction,
    const std::vector<std::vector<int>> &saturated_costs_by_abstraction,
    bool verbose) {
    utils::Timer timer;
    utils::Log() << "Initialize greedy order generator" << endl;

    vector<int> surplus_costs = compute_all_surplus_costs(
        costs, saturated_costs_by_abstraction);
    utils::Log() << "Done computing surplus costs" << endl;

    utils::Log() << "Compute stolen costs" << endl;
    int num_abstractions = abstractions.size();
    vector<int> stolen_costs_by_abstraction;
    stolen_costs_by_abstraction.resize(num_abstractions);
    for (int abs = 0; abs < num_abstractions; ++abs) {
        int sum_stolen_costs = compute_costs_stolen_by_heuristic(
            saturated_costs_by_abstraction[abs], surplus_costs);
        stolen_costs_by_abstraction.push_back(sum_stolen_costs);
    }
    utils::Log() << "Time for initializing greedy order generator: "
                 << timer << endl;

    utils::Timer greedy_timer;
    Order order = get_default_order(num_abstractions);
    // Shuffle order to break ties randomly.
    rng->shuffle(order);
    vector<double> scores;
    scores.reserve(num_abstractions);
    for (int abs = 0; abs < num_abstractions; ++abs) {
        scores.push_back(
            rate_abstraction(
                abstractions[abs], h_values_by_abstraction[abs], stolen_costs_by_abstraction[abs]));
    }
    sort(order.begin(), order.end(), [&](int abs1, int abs2) {
             return scores[abs1] > scores[abs2];
         });
    if (verbose) {
        cout << "Static greedy scores: " << scores << endl;
        unordered_set<double> unique_scores(scores.begin(), scores.end());
        cout << "Static greedy unique scores: " << unique_scores.size() << endl;
        cout << "Static greedy order: " << order << endl;
    }

    if (verbose) {
        utils::Log() << "Time for computing greedy order: " << greedy_timer << endl;
    }

    assert(order.size() == abstractions.size());
    return order;
}


static shared_ptr<MASOrderGenerator> _parse_greedy(OptionParser &parser) {
    add_scoring_function_to_parser(parser);
    add_common_order_generator_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<MASOrderGeneratorGreedy>(opts);
}

static Plugin<MASOrderGenerator> _plugin_greedy("mas_greedy_orders", _parse_greedy);
}
