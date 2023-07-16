#include "saturated_cost_partitioning.h"

#include "merge_and_shrink_representation.h"
#include "order_generator.h"
#include "saturated_cost_partitioning_utils.h"
#include "transition_system.h"
#include "types.h"

#include "../task_proxy.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"
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
        if (abstract_state == PRUNED_STATE) {
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
    const plugins::Options &opts)
    : CostPartitioningFactory(),
      order_generator(
          opts.get<shared_ptr<OrderGenerator>>("order_generator")) {
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate_for_order(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    const vector<int> &order,
    utils::LogProxy &log) const {
    assert(order.size() == abstractions.size());
    int num_labels = label_costs.size();
    vector<AbstractionInformation> abstraction_infos;
    abstraction_infos.reserve(abstractions.size());
    for (size_t i = 0; i < order.size(); ++i) {
        int index = order[i];
        assert(utils::in_bounds(index, abstractions));
        Abstraction &abstraction = *abstractions[index];
        if (log.is_at_least_debug()) {
            log << endl;
            log << "Abstraction index " << index << endl;
//            abstraction.transition_system->dump_labels_and_transitions();
            log << abstraction.transition_system->tag() << endl;
            log << "Remaining label costs: " << label_costs << endl;
        }
        vector<int> goal_distances = compute_goal_distances_for_abstraction(
            abstraction, label_costs, log);
        if (log.is_at_least_debug()) {
            log << "Distances under remaining costs: " << goal_distances << endl;
        }

        // Only keep "useful" abstractions: abstractions which have non-zero
        // heuristic values or are non-total (map to infinite values). See
        // also comment at add_h_values in saturated_cost_partitionings.cc.
        if (!abstraction.merge_and_shrink_representation->is_total() ||
            any_of(goal_distances.begin(), goal_distances.end(), [](int h) {
                       assert(h != INF);
                       return h > 0;
                   })) {
            AbstractionInformation abstraction_info;
            abstraction_info.goal_distances = goal_distances;
            abstraction_info.mas_representation = move(abstraction.merge_and_shrink_representation);
            abstraction_infos.push_back(move(abstraction_info));
        }

        if (i == order.size() - 1) {
            break;
        }

        vector<int> saturated_label_costs = compute_saturated_costs_for_abstraction(
            abstraction, goal_distances, num_labels, log);

        reduce_costs(label_costs, saturated_label_costs);
    }

    if (log.is_at_least_verbose()) {
        int num_abstractions = abstractions.size();
        int num_useful_abstractions = abstraction_infos.size();
        log << "SCP: useful abstractions: " << num_useful_abstractions << "/"
                     << num_abstractions << " = "
                     << static_cast<double>(num_useful_abstractions) / num_abstractions
                     << endl;
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
    utils::LogProxy &log) {
    if (log.is_at_least_debug()) {
        log << "Generating SCP M&S heuristic for given abstractions..." << endl;
    }

    vector<int> order;
    if (abstractions.size() == 1) {
        order = get_default_order(abstractions.size());
    } else {
        order = order_generator->compute_order(
            abstractions, label_costs, log);
        order_generator->clear_internal_state();
    }

    return generate_for_order(move(label_costs), move(abstractions), order, log);
}

class SaturatedCostPartitioningFactoryFeature : public plugins::TypedFeature<CostPartitioningFactory, SaturatedCostPartitioningFactory> {
public:
    SaturatedCostPartitioningFactoryFeature() : TypedFeature("scp") {
        add_option<shared_ptr<OrderGenerator>>(
            "order_generator",
            "order generator",
            "mas_orders()");
    }
};

static plugins::FeaturePlugin<SaturatedCostPartitioningFactoryFeature> _plugin;
}
