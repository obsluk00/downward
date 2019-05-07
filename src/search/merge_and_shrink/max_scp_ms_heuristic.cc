#include "max_scp_ms_heuristic.h"

#include "distances.h"
#include "factored_transition_system.h"
#include "label_equivalence_relation.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "transition_system.h"
#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"

#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>
#include <utility>

using namespace std;
using utils::ExitCode;

namespace merge_and_shrink {
MaxSCPMSHeuristic::MaxSCPMSHeuristic(const options::Options &opts)
    : Heuristic(opts),
      verbosity(static_cast<Verbosity>(opts.get_enum("verbosity"))) {
    cout << "Initializing maximum SCP merge-and-shrink heuristic..." << endl;
    MergeAndShrinkAlgorithm algorithm(opts);
    FactoredTransitionSystem fts = algorithm.build_factored_transition_system(task_proxy);
    finalize(fts);
    cout << "Done initializing maximum SCP merge-and-shrink heuristic." << endl << endl;
}

void MaxSCPMSHeuristic::finalize_factor(
    FactoredTransitionSystem &fts, int index) {
    auto final_entry = fts.extract_factor(index);
    unique_ptr<MergeAndShrinkRepresentation> mas_representation = move(final_entry.first);
    unique_ptr<Distances> distances = move(final_entry.second);
    if (!distances->are_goal_distances_computed()) {
        const bool compute_init = false;
        const bool compute_goal = true;
        distances->compute_distances(compute_init, compute_goal, verbosity);
    }
    assert(distances->are_goal_distances_computed());
    mas_representation->set_distances(*distances);
    mas_representations.push_back(move(mas_representation));
}

void MaxSCPMSHeuristic::finalize(FactoredTransitionSystem &fts) {
    /*
      TODO: This method has quite a bit of fiddling with aspects of
      transition systems and the merge-and-shrink representation (checking
      whether distances have been computed; computing them) that we would
      like to have at a lower level. See also the TODO in
      factored_transition_system.h on improving the interface of that class
      (and also related classes like TransitionSystem etc).
    */

    int active_factors_count = fts.get_num_active_entries();
    if (verbosity >= Verbosity::NORMAL) {
        cout << "Number of remaining factors: " << active_factors_count << endl;
    }

    // Check if there is an unsolvable factor. If so, use it as heuristic.
    for (int index : fts) {
        if (!fts.is_factor_solvable(index)) {
            mas_representations.reserve(1);
            finalize_factor(fts, index);
            if (verbosity >= Verbosity::NORMAL) {
                cout << fts.get_transition_system(index).tag()
                     << "use this unsolvable factor as heuristic."
                     << endl;
            }
            return;
        }
    }

    // Compute original label costs.
    const Labels &labels = fts.get_labels();
    int num_labels = labels.get_size();
    vector<int> remaining_label_costs;
    remaining_label_costs.reserve(num_labels);
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        int label_cost = -1;
        if (labels.is_current_label(label_no)) {
            label_cost = labels.get_label_cost(label_no);
        }
        remaining_label_costs.push_back(label_cost);
    }
    if (verbosity >= Verbosity::VERBOSE) {
        cout << "Original label costs: " << remaining_label_costs << endl;
    }

    // Iterate over all remaining factors and extract them.
    for (int index : fts) {
        if (verbosity >= Verbosity::VERBOSE) {
            cout << "Considering factor at index " << index << endl;
        }
        auto final_entry = fts.extract_factor(index);
        unique_ptr<MergeAndShrinkRepresentation> mas_representation = move(final_entry.first);
        unique_ptr<Distances> distances = move(final_entry.second);

        const TransitionSystem &ts = fts.get_transition_system(index);
        bool all_goal_states = true;
        for (int state = 0; state < ts.get_size(); ++state) {
            if (!ts.is_goal_state(state)) {
                all_goal_states = false;
                break;
            }
        }
        if (all_goal_states) {
            if (verbosity >= Verbosity::VERBOSE) {
                cout << "Factor consists of goal states only, skipping." << endl;
            }
            continue;
        }

//        cout << "Distances before re-computing them: " << distances->get_goal_distances() << endl;

        const bool compute_init = false;
        const bool compute_goal = true;
        distances->compute_distances(compute_init, compute_goal, verbosity, remaining_label_costs);
        assert(distances->are_goal_distances_computed());
        mas_representation->set_distances(*distances);
        mas_representations.push_back(move(mas_representation));

//        cout << "Distances after re-computing them: " << distances->get_goal_distances() << endl;

        // Compute saturated cost of all labels.
        vector<int> saturated_label_costs(remaining_label_costs.size(), -1);
        for (const GroupAndTransitions &gat : ts) {
            const LabelGroup &label_group = gat.label_group;
            const vector<Transition> &transitions = gat.transitions;
            int group_saturated_cost = MINUSINF;
            for (const Transition &transition :transitions) {
                int src = transition.src;
                int target = transition.target;
                int h_src = distances->get_goal_distance(src);
                int h_target = distances->get_goal_distance(target);
                if (h_target != INF) {
                    int diff = h_src - h_target;
                    group_saturated_cost = max(group_saturated_cost, diff);
                }
            }
            if (group_saturated_cost == MINUSINF) {
                cout << "label group does not lead to any state with finite heuristic value" << endl;
            }
            for (int label_no : label_group) {
                saturated_label_costs[label_no] = group_saturated_cost;
            }
        }
        if (verbosity >= Verbosity::VERBOSE) {
            cout << "Saturated label costs: " << saturated_label_costs << endl;
        }

        // Update remaining label costs.
        for (size_t label_no = 0; label_no < remaining_label_costs.size(); ++label_no) {
            if (remaining_label_costs[label_no] == -1) { // skip reduced labels
                assert(saturated_label_costs[label_no] == -1);
            } else {
                if (saturated_label_costs[label_no] == MINUSINF) {
                    remaining_label_costs[label_no] = INF;
                } else {
                    remaining_label_costs[label_no] =
                        remaining_label_costs[label_no] - saturated_label_costs[label_no];
                    assert(remaining_label_costs[label_no] >= 0);
                }
            }
        }
        if (verbosity >= Verbosity::VERBOSE) {
            cout << "Remaining label costs: " << remaining_label_costs << endl;
        }
    }
    if (verbosity >= Verbosity::NORMAL) {
        cout << "Use all factors in an SCP heuristic." << endl;
    }
}

int MaxSCPMSHeuristic::compute_heuristic(const GlobalState &global_state) {
    State state = convert_global_state(global_state);
    int heuristic = 0;
    for (const unique_ptr<MergeAndShrinkRepresentation> &mas_representation : mas_representations) {
        int cost = mas_representation->get_value(state);
        if (cost == PRUNED_STATE || cost == INF) {
            // If state is unreachable or irrelevant, we encountered a dead end.
            return DEAD_END;
        }
        heuristic += cost;
    }
    return heuristic;
}

static shared_ptr<Heuristic> _parse(options::OptionParser &parser) {
    parser.document_synopsis(
        "Merge-and-shrink heuristic",
        "The maximum heuristic computed over SCP heuristics computed over "
        "M&S abstractions.");

    Heuristic::add_options_to_parser(parser);
    add_merge_and_shrink_algorithm_options_to_parser(parser);
    options::Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    handle_shrink_limit_options_defaults(opts);

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return make_shared<MaxSCPMSHeuristic>(opts);
    }
}

static options::Plugin<Evaluator> _plugin("max_scp_ms", _parse);
}
