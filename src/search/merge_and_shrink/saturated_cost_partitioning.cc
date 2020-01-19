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

vector<int> SaturatedCostPartitioningFactory::compute_saturated_costs_simple(
    const TransitionSystem &ts,
    const vector<int> &goal_distances,
    int num_labels,
    utils::Verbosity verbosity) const {
    static bool dump_if_empty_transitions = true;
    static bool dump_if_infinite_transitions = true;
    // Compute saturated cost of all labels.
    vector<int> saturated_label_costs(num_labels, -1);
    for (GroupAndTransitions gat : ts) {
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
                    int diff = h_src - h_target;
                    group_saturated_cost = max(group_saturated_cost, diff);
                }
            }
            if (verbosity >= utils::Verbosity::VERBOSE && dump_if_infinite_transitions && group_saturated_cost == -INF) {
                dump_if_infinite_transitions = false;
                cout << "label group does not lead to any state with finite heuristic value" << endl;
            }
        }
        for (int label_no : label_group) {
            saturated_label_costs[label_no] = group_saturated_cost;
        }
    }
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Saturated label costs: " << saturated_label_costs << endl;
    }
    return saturated_label_costs;
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate_simple(
    const Labels &labels,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Computing SCP M&S heuristic over current abstractions..." << endl;
    }

    // Compute original label costs.
    int num_labels = labels.get_size();
    vector<int> remaining_label_costs(num_labels, -1);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels.is_current_label(label_no)) {
            remaining_label_costs[label_no] = labels.get_label_cost(label_no);
        }
    }

    vector<vector<int>> goal_distances_by_abstraction;
    vector<vector<int>> saturated_cost_by_abstraction;
    goal_distances_by_abstraction.reserve(abstractions.size());
    saturated_cost_by_abstraction.reserve(abstractions.size());
    for (const unique_ptr<Abstraction> &abs : abstractions) {
        const TransitionSystem &ts = *abs->transition_system;
        goal_distances_by_abstraction.push_back(
            compute_goal_distances(
                ts, remaining_label_costs, verbosity));
        saturated_cost_by_abstraction.push_back(
            compute_saturated_costs_simple(
                ts, goal_distances_by_abstraction.back(), num_labels, verbosity));
    }
    vector<int> abstraction_order = order_generator->compute_order_for_state(
        abstractions, remaining_label_costs, goal_distances_by_abstraction, saturated_cost_by_abstraction, true);

    SCPMSHeuristic scp_ms_heuristic;

    for (size_t i = 0; i < abstraction_order.size(); ++i) {
        int index = abstraction_order[i];
        Abstraction &abstraction = *abstractions[index];
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
        const TransitionSystem &ts = *abstraction.transition_system;
        vector<int> goal_distances = compute_goal_distances(
            ts, remaining_label_costs, verbosity);
//        cout << "Distances under remaining costs: " << goal_distances << endl;
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representations.push_back(move(abstraction.merge_and_shrink_representation));
        if (i == abstraction_order.size() - 1) {
            break;
        }

        // Compute saturated cost of all labels.
        vector<int> saturated_label_costs = compute_saturated_costs_simple(
            ts, goal_distances, num_labels, verbosity);

        // Update remaining label costs.
        for (size_t label_no = 0; label_no < remaining_label_costs.size(); ++label_no) {
            if (remaining_label_costs[label_no] == -1) { // skip reduced labels
                assert(saturated_label_costs[label_no] == -1);
            } else {
                if (saturated_label_costs[label_no] == -INF) {
                    remaining_label_costs[label_no] = INF;
                } else if (remaining_label_costs[label_no] != INF) { // inf remains inf
                    remaining_label_costs[label_no] =
                        remaining_label_costs[label_no] - saturated_label_costs[label_no];
                    assert(remaining_label_costs[label_no] >= 0);
                }
            }
        }
    }

    assert(scp_ms_heuristic.goal_distances.size());

    return utils::make_unique_ptr<SaturatedCostPartitioning>(move(scp_ms_heuristic));
}

vector<int> SaturatedCostPartitioningFactory::compute_goal_distances_different_labels(
    const TransitionSystem &ts,
    int num_original_labels,
    const vector<int> &remaining_label_costs,
    const vector<int> &label_mapping,
    utils::Verbosity verbosity) const {
    vector<int> abs_label_costs(num_original_labels * 2, -1);
//        set<int> abs_labels;
    for (int label_no = 0; label_no < num_original_labels; ++label_no) {
        int label_cost = remaining_label_costs[label_no];
        assert(label_cost >= 0);
        int abs_label_no = label_mapping[label_no];
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
        ts, abs_label_costs, verbosity);
    return goal_distances;
}

vector<int> SaturatedCostPartitioningFactory::compute_saturated_costs_different_labels(
    const TransitionSystem &ts,
    const vector<int> &goal_distances,
    int num_original_labels,
    const vector<vector<int>> &reduced_to_original_labels,
    const vector<int> &remaining_label_costs,
    utils::Verbosity verbosity) const {
    static bool dump_if_empty_transitions = true;
    static bool dump_if_infinite_transitions = true;
    // Compute saturated cost of all labels.
    vector<int> saturated_label_costs(num_original_labels, -1);
//        set<int> mapped_labels;
    for (GroupAndTransitions gat : ts) {
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
        for (int abs_label_no : label_group) {
//                assert(abs_labels.count(abs_label_no));
            for (int original_label_no : reduced_to_original_labels.at(abs_label_no)) {
//                    assert(!mapped_labels.count(original_label_no));
//                    mapped_labels.insert(original_label_no);
                assert(group_saturated_cost <= remaining_label_costs[original_label_no]);
                saturated_label_costs[original_label_no] = group_saturated_cost;

            }
        }
    }
//        cout << "num original labels in abs: " << mapped_labels.size() << endl;
//        assert(static_cast<int>(mapped_labels.size()) == num_original_labels);
//        cout << "original labels from abs: "
//             << vector<int>(mapped_labels.begin(), mapped_labels.end()) << endl;
//        assert(original_labels == vector<int>(mapped_labels.begin(), mapped_labels.end()));
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Saturated label costs: " << saturated_label_costs << endl;
    }
    return saturated_label_costs;
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::generate_over_different_labels(
    vector<int> &&original_labels,
    vector<int> &&label_costs,
    vector<vector<int>> &&label_mappings,
    vector<vector<int>> &&reduced_to_original_labels,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Computing SCP M&S heuristic over current abstractions..." << endl;
    }

    int num_original_labels = original_labels.size();
    vector<int> remaining_label_costs(move(label_costs));

    vector<vector<int>> goal_distances_by_abstraction;
    vector<vector<int>> saturated_cost_by_abstraction;
    goal_distances_by_abstraction.reserve(abstractions.size());
    saturated_cost_by_abstraction.reserve(abstractions.size());
    for (size_t i = 0; i < abstractions.size(); ++i) {
        const Abstraction &abs = *abstractions[i];
        const TransitionSystem &ts = *abs.transition_system;
        goal_distances_by_abstraction.push_back(compute_goal_distances_different_labels(
            ts, num_original_labels, remaining_label_costs, label_mappings[i], verbosity));
        saturated_cost_by_abstraction.push_back(compute_saturated_costs_different_labels(
            ts, goal_distances_by_abstraction.back(), num_original_labels, reduced_to_original_labels,
            remaining_label_costs, verbosity));
    }
    vector<int> abstraction_order = order_generator->compute_order_for_state(
        abstractions, remaining_label_costs, goal_distances_by_abstraction, saturated_cost_by_abstraction, true);

    SCPMSHeuristic scp_ms_heuristic;
    for (size_t i = 0; i < abstraction_order.size(); ++i) {
        size_t index = abstraction_order[i];
        Abstraction &abstraction = *abstractions[index];
        const TransitionSystem &ts = *abstraction.transition_system;

        const vector<int> &label_mapping = label_mappings[index];
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << endl;
            cout << "Abstraction index " << index << endl;
//            ts.dump_labels_and_transitions();
            cout << ts.tag() << endl;
            cout << "Label mapping: " << label_mapping << endl;
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
        vector<int> goal_distances = compute_goal_distances_different_labels(
            ts, num_original_labels, remaining_label_costs, label_mapping, verbosity);
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Distances under remaining costs: " << goal_distances << endl;
        }
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representations.push_back(move(abstraction.merge_and_shrink_representation));
        if (i == abstraction_order.size() - 1) {
            break;
        }

        vector<int> saturated_label_costs = compute_saturated_costs_different_labels(
            ts, goal_distances, num_original_labels, reduced_to_original_labels,
            remaining_label_costs, verbosity);

        // Update remaining label costs.
        for (int label_no = 0; label_no < num_original_labels; ++label_no) {
            assert(remaining_label_costs[label_no] != -1);
            if (saturated_label_costs[label_no] == -INF) {
                remaining_label_costs[label_no] = INF;
            } else if (remaining_label_costs[label_no] != INF) { // inf remains inf
                remaining_label_costs[label_no] =
                    remaining_label_costs[label_no] - saturated_label_costs[label_no];
                assert(remaining_label_costs[label_no] >= 0);
            }
        }
    }

    assert(scp_ms_heuristic.goal_distances.size());

    for (auto &abs : abstractions) {
        delete abs->transition_system;
        abs->transition_system = nullptr;
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
