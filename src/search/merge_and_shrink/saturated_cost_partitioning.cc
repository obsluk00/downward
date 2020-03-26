#include "saturated_cost_partitioning.h"

#include "merge_and_shrink_representation.h"
#include "order_generator.h"
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
    vector<AbstractionInformation> &&abstraction_infos)
    : CostPartitioning(),
      abstraction_infos(move(abstraction_infos)) {
}

int SaturatedCostPartitioning::compute_value(const State &state) {
    int h_val = 0;
    for (const AbstractionInformation &abstraction_info : abstraction_infos) {
        int abstract_state = abstraction_info.mas_representation->get_value(state);
        if (abstract_state == PRUNED_STATE)  {
            // If the state has been pruned, we encountered a dead end.
            return INF;
        }
        int cost = abstraction_info.goal_distances[abstract_state];
        if (cost == INF) {
            // If the state is unreachable or irrelevant, we encountered a dead end.
            return INF;
        }
        h_val += cost;
    }
    return h_val;
}

int SaturatedCostPartitioning::get_number_of_abstractions() const {
    return abstraction_infos.size();
}

SaturatedCostPartitioningFactory::SaturatedCostPartitioningFactory(
    const Options &opts)
    : CostPartitioningFactory(),
      order_generator(
        opts.get<shared_ptr<OrderGenerator>>("order_generator")) {
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate_for_order(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    const vector<int> &order,
    utils::Verbosity verbosity) const {
    assert(order.size() == abstractions.size());
    int num_labels = label_costs.size();
    vector<AbstractionInformation> abstraction_infos;
    abstraction_infos.reserve(abstractions.size());
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
        AbstractionInformation abstraction_info;
        abstraction_info.goal_distances = goal_distances;
        abstraction_info.mas_representation = move(abstraction.merge_and_shrink_representation);
        abstraction_infos.push_back(move(abstraction_info));
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

    return utils::make_unique_ptr<SaturatedCostPartitioning>(move(abstraction_infos));
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Generating SCP M&S heuristic for given abstractions..." << endl;
    }

    order_generator->clear_internal_state();
    vector<int> order = order_generator->compute_order(
        abstractions, label_costs, verbosity);

    return generate_for_order(move(label_costs), move(abstractions), order, verbosity);
}

static shared_ptr<SaturatedCostPartitioningFactory>_parse(OptionParser &parser) {
    parser.add_option<shared_ptr<OrderGenerator>>(
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
