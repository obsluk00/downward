#include "saturated_cost_partitioning.h"

#include "distances.h"
#include "factored_transition_system.h"
#include "label_equivalence_relation.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "transition_system.h"
#include "types.h"

#include "../plugin.h"

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {
SaturatedCostPartitioning::SaturatedCostPartitioning(SCPMSHeuristic scp_ms_heuristic)
    : CostPartitioning(),
      scp_ms_heuristic(scp_ms_heuristic) {
}

SaturatedCostPartitioning::~SaturatedCostPartitioning() {
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

SaturatedCostPartitioningFactory::SaturatedCostPartitioningFactory(
    const options::Options &opts)
    : CostPartitioningFactory(),
      options(opts),
      rng(utils::parse_rng_from_options(opts)),
      factor_order(static_cast<FactorOrder>(opts.get_enum("factor_order"))),
      verbosity(static_cast<utils::Verbosity>(opts.get_enum("verbosity"))) {
}

SaturatedCostPartitioningFactory::~SaturatedCostPartitioningFactory() {
}

vector<unique_ptr<CostPartitioning>> SaturatedCostPartitioningFactory::generate(const TaskProxy &task_proxy) const {
    MergeAndShrinkAlgorithm algorithm(options);
    vector<unique_ptr<CostPartitioning>> result;
    FTSSnapshotCollector fts_snapshot_collector(
        options.get<bool>("compute_atomic_snapshot"),
        options.get<bool>("compute_final_snapshot"),
        options.get<int>("main_loop_target_num_snapshots"),
        options.get<int>("main_loop_snapshot_each_iteration"),
        [this,result](const FactoredTransitionSystem &fts) {
            result.push_back(compute_scp_over_fts(fts));
        },
        verbosity);
    FactoredTransitionSystem fts = algorithm.build_factored_transition_system(
        task_proxy, &fts_snapshot_collector);
    bool unsolvable = false;
    for (int index : fts) {
        if (!fts.is_factor_solvable(index)) {
            result.clear();
            result.reserve(1);
            result.push_back(utils::make_unique_ptr<SaturatedCostPartitioning>(extract_scp_heuristic(fts, index)));
            unsolvable= true;
            break;
        }
    }

    if (!unsolvable) {
        fts_snapshot_collector.report_final_snapshot(fts);
    }

    /*int sum_of_factors = 0;
    for (const SCPMSHeuristic &scp_ms_heuristic : scp_ms_heuristics) {
        sum_of_factors += scp_ms_heuristic.mas_representations.size();
    }
    double average_number_of_factors_per_scp_ms_heuristic =
        sum_of_factors / static_cast<double>(num_cps);
    cout << "Number of SCP merge-and-shrink heuristics: "
         << num_cps << endl;
    cout << "Average number of factors per SCP merge-and-shrink heuristic: "
         << average_number_of_factors_per_scp_ms_heuristic << endl;*/

    return result;
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningFactory::compute_scp_over_fts(
    const FactoredTransitionSystem &fts) const {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Computing SCP M&S heuristic over current FTS..." << endl;
    }

    // Compute original label costs.
    const Labels &labels = fts.get_labels();
    int num_labels = labels.get_size();
    vector<int> remaining_label_costs(num_labels, -1);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        if (labels.is_current_label(label_no)) {
            remaining_label_costs[label_no] = labels.get_label_cost(label_no);
        }
    }

    vector<int> active_factor_indices;
    active_factor_indices.reserve(fts.get_num_active_entries());
    for (int index : fts) {
        active_factor_indices.push_back(index);
    }
    if (factor_order == FactorOrder::RANDOM) {
        rng->shuffle(active_factor_indices);
    }

    SCPMSHeuristic scp_ms_heuristic;
    bool dump_if_empty_transitions = true;
    bool dump_if_infinite_transitions = true;
    for (size_t i = 0; i < active_factor_indices.size(); ++i) {
        int index = active_factor_indices[i];
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Considering factor at index " << index << endl;
        }

        if (fts.is_factor_trivial(index)) {
            if (verbosity >= utils::Verbosity::DEBUG) {
                cout << "factor is trivial" << endl;
            }
            continue;
        }

//        const Distances &distances = fts.get_distances(index);
//        cout << "Distances under full costs: " << distances.get_goal_distances() << endl;
        if (verbosity >= utils::Verbosity::DEBUG) {
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
        const TransitionSystem &ts = fts.get_transition_system(index);
        vector<int> goal_distances = compute_goal_distances(
            ts, remaining_label_costs, verbosity);
//        cout << "Distances under remaining costs: " << goal_distances << endl;
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
        scp_ms_heuristic.goal_distances.push_back(goal_distances);
        scp_ms_heuristic.mas_representations.push_back(move(mas_representation));
        if (i == active_factor_indices.size() - 1) {
            break;
        }

        // Compute saturated cost of all labels.
        vector<int> saturated_label_costs(remaining_label_costs.size(), -1);
        for (const GroupAndTransitions &gat : ts) {
            const LabelGroup &label_group = gat.label_group;
            const vector<Transition> &transitions = gat.transitions;
            int group_saturated_cost = MINUSINF;
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
                if (verbosity >= utils::Verbosity::VERBOSE && dump_if_infinite_transitions && group_saturated_cost == MINUSINF) {
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

        // Update remaining label costs.
        for (size_t label_no = 0; label_no < remaining_label_costs.size(); ++label_no) {
            if (remaining_label_costs[label_no] == -1) { // skip reduced labels
                assert(saturated_label_costs[label_no] == -1);
            } else {
                if (saturated_label_costs[label_no] == MINUSINF) {
                    remaining_label_costs[label_no] = INF;
                } else if (remaining_label_costs[label_no] != INF) { // inf remains inf
                    remaining_label_costs[label_no] =
                        remaining_label_costs[label_no] - saturated_label_costs[label_no];
                    assert(remaining_label_costs[label_no] >= 0);
                }
            }
        }
    }

    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Done computing SCP M&S heuristic over current FTS." << endl;
    }

    return utils::make_unique_ptr<SaturatedCostPartitioning>(scp_ms_heuristic);
}

SCPMSHeuristic SaturatedCostPartitioningFactory::extract_scp_heuristic(
    FactoredTransitionSystem &fts, int index) const {
    SCPMSHeuristic scp_ms_heuristic;
    scp_ms_heuristic.goal_distances.reserve(1);
    scp_ms_heuristic.mas_representations.reserve(1);
    auto factor = fts.extract_factor(index);
    scp_ms_heuristic.goal_distances.push_back(factor.second->get_goal_distances());
    scp_ms_heuristic.mas_representations.push_back(move(factor.first));
    return scp_ms_heuristic;
}

static shared_ptr<SaturatedCostPartitioningFactory>_parse(options::OptionParser &parser) {
    utils::add_rng_options(parser);

    vector<string> factor_order;
    vector<string> factor_order_docs;
    factor_order.push_back("given");
    factor_order_docs.push_back(
        "given: the order of factors as in the FTS");
    factor_order.push_back("random");
    factor_order_docs.push_back(
        "random: random order of factors");
    parser.add_enum_option(
        "factor_order",
        factor_order,
        "Option to specify the order in which factors of the FTS are "
        "considered for computing the SCP.",
        "random",
        factor_order_docs);

    parser.add_option<bool>(
        "compute_atomic_snapshot",
        "Include an SCP heuristic computed over the atomic FTS.",
        "false");
    parser.add_option<bool>(
        "compute_final_snapshot",
        "Include an SCP heuristic computed over the final FTS (attention: "
        "depending on main_loop_target_num_snapshots, this might already have "
        "been computed).",
        "false");
    parser.add_option<int>(
        "main_loop_target_num_snapshots",
        "The aimed number of SCP heuristics to be computed over the main loop.",
        "0",
        Bounds("0", "infinity"));
    parser.add_option<int>(
        "main_loop_snapshot_each_iteration",
        "A number of iterations after which an SCP heuristic is computed over "
        "the current FTS.",
        "0",
        Bounds("0", "infinity"));

    add_merge_and_shrink_algorithm_options_to_parser(parser);
    options::Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    handle_shrink_limit_options_defaults(opts);

    bool compute_atomic_snapshot = opts.get<bool>("compute_atomic_snapshot");
    bool compute_final_snapshot = opts.get<bool>("compute_final_snapshot");
    int main_loop_target_num_snapshots = opts.get<int>("main_loop_target_num_snapshots");
    int main_loop_snapshot_each_iteration =
        opts.get<int>("main_loop_snapshot_each_iteration");
    if (!compute_atomic_snapshot &&
        !compute_final_snapshot &&
        !main_loop_target_num_snapshots &&
        !main_loop_snapshot_each_iteration) {
        cerr << "At least one option for computing SCP merge-and-shrink "
                "heuristics must be enabled! " << endl;
        if (main_loop_target_num_snapshots && main_loop_snapshot_each_iteration) {
            cerr << "Can't set both the number of heuristics and the iteration "
                    "offset in which heuristics are computed."
                 << endl;
        }
        utils::exit_with(utils::ExitCode::SEARCH_INPUT_ERROR);
    }

    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<SaturatedCostPartitioningFactory>(opts);
}

static options::Plugin<CostPartitioningFactory> _plugin("scp", _parse);
}
