#include "saturated_cost_partitioning.h"

#include "merge_and_shrink_representation.h"
#include "single_use_order_generator.h"
#include "saturated_cost_partitioning_utils.h"
#include "transition_system.h"
#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {
SaturatedCostPartitioning::SaturatedCostPartitioning(
    SCPMSHeuristic &&scp_ms_heuristic)
    : CostPartitioning(),
      scp_ms_heuristic(move(scp_ms_heuristic)) {
}

int SaturatedCostPartitioning::compute_value(const State &state) {
    int h_val = 0;
    assert(scp_ms_heuristic.mas_representations.size() == scp_ms_heuristic.goal_distances.size());
    for (size_t factor_index = 0; factor_index < scp_ms_heuristic.mas_representations.size(); ++factor_index) {
        int abstract_state = scp_ms_heuristic.mas_representations[factor_index]->get_value(state);
        if (abstract_state == PRUNED_STATE)  {
            // If the state has been pruned, we encountered a dead end.
            return INF;
        }
        int cost = scp_ms_heuristic.goal_distances[factor_index][abstract_state];
        if (cost == INF) {
            // If the state is unreachable or irrelevant, we encountered a dead end.
            return INF;
        }
        h_val += cost;
    }
    return h_val;
}

int SaturatedCostPartitioning::get_number_of_factors() const {
    return scp_ms_heuristic.goal_distances.size();
}

SaturatedCostPartitioningFactory::SaturatedCostPartitioningFactory(
    const Options &opts)
    : CostPartitioningFactory(),
      order_generator(opts.get<shared_ptr<MASOrderGenerator>>("order_generator")) {
}

void SaturatedCostPartitioningFactory::initialize(const TaskProxy &task_proxy) {
    order_generator->initialize(task_proxy);
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate_for_order(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    const vector<int> order,
    utils::Verbosity verbosity) const {
    int num_labels = label_costs.size();
    SCPMSHeuristic scp_ms_heuristic;
    for (size_t i = 0; i < order.size(); ++i) {
        int index = order[i];
        Abstraction &abstraction = *abstractions[index];
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << endl;
            cout << "Abstraction index " << index << endl;
//            abstraction.transition_system->dump_labels_and_transitions();
            cout << abstraction.transition_system->tag() << endl;
            cout << "Remaining label costs: " << label_costs << endl;
        }
        vector<int> goal_distances = compute_goal_distances_for_abstraction(
            abstraction, label_costs, verbosity);
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Distances under remaining costs: " << goal_distances << endl;
        }
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representations.push_back(move(abstraction.merge_and_shrink_representation));
        if (i == order.size() - 1) {
            break;
        }

        vector<int> saturated_label_costs = compute_saturated_costs_for_abstraction(
            abstraction, goal_distances, num_labels, verbosity);

        reduce_costs(label_costs, saturated_label_costs);
    }

    // Release copied transition systems if we are in an offline scenario.
    for (auto &abs : abstractions) {
        if (!abs->label_mapping.empty()) {
            delete abs->transition_system;
            abs->transition_system = nullptr;
        }
    }

    return utils::make_unique_ptr<SaturatedCostPartitioning>(move(scp_ms_heuristic));
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Generating SCP M&S heuristic for given abstractions..." << endl;
    }

    vector<int> order = order_generator->compute_order_for_state(
        abstractions, label_costs, verbosity);

    return generate_for_order(move(label_costs), move(abstractions), order, verbosity);
}

static shared_ptr<SaturatedCostPartitioningFactory>_parse(OptionParser &parser) {
    parser.add_option<shared_ptr<MASOrderGenerator>>(
        "order_generator",
        "order generator",
        "mas_orders()");

    Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<SaturatedCostPartitioningFactory>(opts);
}

static Plugin<CostPartitioningFactory> _plugin("scp", _parse);
}
