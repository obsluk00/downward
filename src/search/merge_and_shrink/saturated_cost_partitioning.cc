#include "saturated_cost_partitioning.h"

#include "distances.h"
#include "factored_transition_system.h"
#include "label_equivalence_relation.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "order_generator.h"
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
vector<int> compute_goal_distances_for_abstraction(
    const Abstraction &abstraction, const vector<int> &label_costs, utils::Verbosity verbosity) {
    if (abstraction.label_mapping.empty()) {
        return compute_goal_distances(
             *abstraction.transition_system, label_costs, verbosity);
    }
    int num_labels = label_costs.size();
    vector<int> abs_label_costs(num_labels * 2, -1);
//        set<int> abs_labels;
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        int label_cost = label_costs[label_no];
        assert(label_cost >= 0);
        int abs_label_no = abstraction.label_mapping[label_no];
        if (abs_label_costs[abs_label_no] != -1) {
            abs_label_costs[abs_label_no] = min(abs_label_costs[abs_label_no], label_cost);
        } else {
            abs_label_costs[abs_label_no] = label_cost;
        }
//            abs_labels.insert(abs_label_no);
    }
    if (verbosity >= utils::Verbosity::DEBUG) {
//            cout << "Abs labels: " << vector<int>(abs_labels.begin(), abs_labels.end()) << endl;
        cout << "Remaining label costs in abs: " << abs_label_costs << endl;
    }

    vector<int> goal_distances = compute_goal_distances(
        *abstraction.transition_system, abs_label_costs, verbosity);
    return goal_distances;
}

vector<vector<int>> compute_inverse_label_mapping(const Abstraction &abstractions) {
    assert(!abstractions.label_mapping.empty());
    // TODO: taking twice the size is pessimistic; maybe resize dynamically?
    vector<vector<int>> reduced_to_orignal_labels(abstractions.label_mapping.size() * 2);
    for (size_t original_label = 0; original_label < abstractions.label_mapping.size(); ++original_label) {
        int reduced_label = abstractions.label_mapping[original_label];
        reduced_to_orignal_labels[reduced_label].push_back(original_label);
    }
    return reduced_to_orignal_labels;
}

vector<int> compute_saturated_costs_for_abstraction(
    const Abstraction &abstraction,
    const vector<int> &goal_distances,
    int num_labels,
    utils::Verbosity verbosity) {
    static bool dump_if_empty_transitions = true;
    static bool dump_if_infinite_transitions = true;
    vector<vector<int>> reduced_to_original_labels;
    if (!abstraction.label_mapping.empty()) {
        reduced_to_original_labels = compute_inverse_label_mapping(abstraction);
    }
    vector<int> saturated_label_costs(num_labels, -1);
//    set<int> mapped_labels;
    for (GroupAndTransitions gat : *abstraction.transition_system) {
        const LabelGroup &label_group = gat.label_group;
        const vector<Transition> &transitions = gat.transitions;
        int group_saturated_cost = -INF;
        if (verbosity >= utils::Verbosity::VERBOSE && dump_if_empty_transitions && transitions.empty()) {
            dump_if_empty_transitions = false;
            cout << "found dead label group" << endl;
        } else {
            for (const Transition &transition : transitions) {
                int src = transition.src;
                int target = transition.target;
                int h_src = goal_distances[src];
                int h_target = goal_distances[target];
                if (h_target != INF) {
                    // h_src = INF is possible for transitions with labels
                    // that all have infinite costs.
                    int diff = h_src - h_target;
                    group_saturated_cost = max(group_saturated_cost, diff);
                }
            }
            if (verbosity >= utils::Verbosity::VERBOSE
                && dump_if_infinite_transitions
                && group_saturated_cost == -INF) {
                dump_if_infinite_transitions = false;
                cout << "label group does not lead to any state with finite heuristic value" << endl;
            }
        }
        for (int label_no : label_group) {
            if (!reduced_to_original_labels.empty()) {
                for (int original_label_no : reduced_to_original_labels.at(label_no)) {
//                    assert(!mapped_labels.count(original_label_no));
//                    mapped_labels.insert(original_label_no);
                    saturated_label_costs[original_label_no] = group_saturated_cost;
                }
            } else {
                saturated_label_costs[label_no] = group_saturated_cost;
            }
        }
    }
//    cout << "num original labels in abs: " << mapped_labels.size() << endl;
//    assert(static_cast<int>(mapped_labels.size()) == num_original_labels);
//    cout << "original labels from abs: "
//         << vector<int>(mapped_labels.begin(), mapped_labels.end()) << endl;
//    assert(original_labels == vector<int>(mapped_labels.begin(), mapped_labels.end()));
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Saturated label costs: " << saturated_label_costs << endl;
    }
    return saturated_label_costs;
}

void reduce_costs(vector<int> &label_costs, const vector<int> &saturated_label_costs) {
    for (size_t label_no = 0; label_no < label_costs.size(); ++label_no) {
        if (label_costs[label_no] == -1) { // skip reduced labels
            assert(saturated_label_costs[label_no] == -1);
        } else {
            if (saturated_label_costs[label_no] == -INF) {
                label_costs[label_no] = INF;
            } else if (label_costs[label_no] != INF) { // inf remains inf
                label_costs[label_no] =
                    label_costs[label_no] - saturated_label_costs[label_no];
                assert(label_costs[label_no] >= 0);
            }
        }
    }
}

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

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Computing SCP M&S heuristic over current abstractions..." << endl;
    }

    int num_labels = label_costs.size();
    vector<int> remaining_label_costs(move(label_costs));

    vector<vector<int>> goal_distances_by_abstraction;
    vector<vector<int>> saturated_cost_by_abstraction;
    goal_distances_by_abstraction.reserve(abstractions.size());
    saturated_cost_by_abstraction.reserve(abstractions.size());
    for (const unique_ptr<Abstraction> &abstraction : abstractions) {
        goal_distances_by_abstraction.push_back(
            compute_goal_distances_for_abstraction(
                *abstraction, remaining_label_costs, verbosity));
        saturated_cost_by_abstraction.push_back(
            compute_saturated_costs_for_abstraction(
                *abstraction, goal_distances_by_abstraction.back(), num_labels, verbosity));
    }
    vector<int> abstraction_order = order_generator->compute_order_for_state(
        abstractions, remaining_label_costs, goal_distances_by_abstraction, saturated_cost_by_abstraction, true);

    SCPMSHeuristic scp_ms_heuristic;
    for (size_t i = 0; i < abstraction_order.size(); ++i) {
        int index = abstraction_order[i];
        Abstraction &abstraction = *abstractions[index];
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << endl;
            cout << "Abstraction index " << index << endl;
//            abstraction.transition_system->dump_labels_and_transitions();
            cout << abstraction.transition_system->tag() << endl;
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
        vector<int> goal_distances = compute_goal_distances_for_abstraction(
            abstraction, remaining_label_costs, verbosity);
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Distances under remaining costs: " << goal_distances << endl;
        }
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representations.push_back(move(abstraction.merge_and_shrink_representation));
        if (i == abstraction_order.size() - 1) {
            break;
        }

        vector<int> saturated_label_costs = compute_saturated_costs_for_abstraction(
            abstraction, goal_distances, num_labels, verbosity);

        reduce_costs(remaining_label_costs, saturated_label_costs);
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
