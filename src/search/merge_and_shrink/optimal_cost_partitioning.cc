#include "optimal_cost_partitioning.h"

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

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {
int AbstractionInformation::get_local_op_cost_variable(int op_id) const {
    return local_op_cost_offset + op_id;
}

int AbstractionInformation::get_state_cost_variable(int state_id) const {
    return state_cost_offset + state_id;
}

OptimalCostPartitioning::OptimalCostPartitioning(
    std::vector<AbstractionInformation> &&abstractions,
    std::unique_ptr<lp::LPSolver> lp_solver)
    : CostPartitioning(),
      abstractions(move(abstractions)),
      lp_solver(move(lp_solver)) {
}

bool OptimalCostPartitioning::set_current_state(const State &state) {
    // Change objective to maximize heuristic value of initial states in all projections.
    for (AbstractionInformation &abstraction_info : abstractions) {
        // Unset previous state.
        lp_solver->set_objective_coefficient(abstraction_info.variable_in_objective, 0);

        int abstract_state = abstraction_info.abstraction_function->get_value(state);
        if (abstract_state == PRUNED_STATE) {
            return false;
        }
        int var_id = abstraction_info.get_state_cost_variable(abstract_state);
        lp_solver->set_objective_coefficient(var_id, 1);
        abstraction_info.variable_in_objective = var_id;
    }
    return true;
}

int OptimalCostPartitioning::compute_value(const State &state) {
    if (!set_current_state(state)) {
        return INF;
    }

//    utils::Timer solve_time;
    lp_solver->solve();

    /*static bool first = true;
    if (first) {
        cout << "LP solve time: " << solve_time << endl;
        cout << "LP peak memory after solve: " << utils::get_peak_memory_in_kb() << endl;

        first = false;
    }*/

    if (lp_solver->has_optimal_solution()) {
        double heuristic_value = lp_solver->get_objective_value();
        const double epsilon = 0.01;
        return static_cast<int>(ceil(heuristic_value - epsilon));
    } else {
        return INF;
    }
}

int OptimalCostPartitioning::get_number_of_factors() const {
    return abstractions.size();
}

OptimalCostPartitioningFactory::OptimalCostPartitioningFactory(
    const Options &opts)
    : CostPartitioningFactory(),
      lp_solver_type(lp::LPSolverType(opts.get_enum("lpsolver"))),
      allow_negative_costs(opts.get<bool>("allow_negative_costs")),
      efficient_cp(opts.get<bool>("efficient_cp")) {
}

void OptimalCostPartitioningFactory::create_abstraction_variables(
    vector<lp::LPVariable> &variables,
    double infinity,
    AbstractionInformation &abstraction_info,
    int num_states,
    int num_labels) {

    // Create variables for local operator cost.
    abstraction_info.local_op_cost_offset = variables.size();
    for (int label_no = 0; label_no < num_labels; ++label_no) {
        double lower_bound = 0;
        if (allow_negative_costs) {
            lower_bound = -infinity;
        }
        variables.emplace_back(lower_bound, infinity, 0);
    }

    // Create variables for abstract state heuristic values.
    abstraction_info.state_cost_offset = variables.size();
    // Pretend the first state is in the objective, so the first heuristic evaluation has something to unset.
    abstraction_info.variable_in_objective = abstraction_info.state_cost_offset;
    for (int i = 0; i < num_states; ++i) {
        variables.emplace_back(-infinity, infinity, 0);
    }
}

void OptimalCostPartitioningFactory::create_abstraction_constraints(
    vector<lp::LPVariable> &variables,
    vector<lp::LPConstraint> &constraints,
    double infinity,
    const AbstractionInformation &abstraction_info,
    const TransitionSystem &transition_system,
    const vector<int> &contiguous_label_mapping) const {

    // Add constraints: H_alpha(g) <= 0 (as variable lower bounds) for all abstract goals g.
    for (int state = 0; state < transition_system.get_size(); ++state) {
        if (transition_system.is_goal_state(state)) {
            int goal_var = abstraction_info.get_state_cost_variable(state);
            variables[goal_var].upper_bound = 0;
        }
    }

    for (GroupAndTransitions gat : transition_system) {
        bool have_set_lower_bound = false;
        for (const Transition &transition : gat.transitions) {
            if (transition.src != transition.target) {
                // Create constraints for state-changing transitions.
                int source_var = abstraction_info.get_state_cost_variable(transition.src);
                int target_var = abstraction_info.get_state_cost_variable(transition.target);
                for (int label_no : gat.label_group) {
                    int op_var = abstraction_info.get_local_op_cost_variable(
                            contiguous_label_mapping[label_no]);

                    // Add constraint: H_alpha(s) <= H_alpha(s') + C_alpha(o)
                    lp::LPConstraint constraint(0, infinity);
                    constraint.insert(source_var, -1);
                    constraint.insert(op_var, 1);
                    constraint.insert(target_var, 1);
                    constraints.push_back(constraint);
                }
            } else {
                if (!have_set_lower_bound) {
                    /*
                      Self loops are a special case of transitions that can be treated more
                      efficiently, because the variables H_alpha(s) and H_alpha(s') cancel out.
                    */
                    for (int label_no : gat.label_group) {
                        int op_var = abstraction_info.get_local_op_cost_variable(
                                contiguous_label_mapping[label_no]);
                        variables[op_var].lower_bound = 0;
                    }
                    have_set_lower_bound = true;
                }
            }
        }
    }
}

void OptimalCostPartitioningFactory::create_global_constraints(
    vector<lp::LPConstraint> &constraints,
    const Labels &labels,
    vector<int> &contiguous_label_mapping,
    const std::vector<AbstractionInformation> &abstractions) const {
    // Create cost partitioning constraints.
    for (int label_no = 0; label_no < labels.get_size(); ++label_no) {
        if (labels.is_current_label(label_no)) {
            // Add constraint: sum_alpha Cost_alpha(o) <= cost(o)
            lp::LPConstraint constraint(0, labels.get_label_cost(label_no));
            for (const AbstractionInformation &abstraction_info : abstractions) {
                constraint.insert(abstraction_info.get_local_op_cost_variable(
                        contiguous_label_mapping[label_no]), 1);
            }
            constraints.push_back(constraint);
        }
    }
}

unique_ptr<CostPartitioning> OptimalCostPartitioningFactory::generate(
    FactoredTransitionSystem &fts,
    utils::Verbosity verbosity,
    int unsolvable_index) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Computing OCP M&S heuristic over current FTS..." << endl;
        cout << "LP peak memory before construct: " << utils::get_peak_memory_in_kb() << endl;
    }

    vector<int> active_factor_indices;
    if (unsolvable_index == -1) {
        active_factor_indices.reserve(fts.get_num_active_entries());
        for (int index : fts) {
            active_factor_indices.push_back(index);
        }
    } else {
        active_factor_indices.reserve(1);
        active_factor_indices.push_back(unsolvable_index);
    }

    const Labels &labels = fts.get_labels();
    int largest_label_no = labels.get_size();
    // Contiguous renumbering of labels.
    vector<int> contiguous_label_mapping(largest_label_no, -1);
    int num_labels = 0;
    for (int label_no = 0; label_no < largest_label_no; ++label_no) {
        if (labels.is_current_label(label_no)) {
            contiguous_label_mapping[label_no] = num_labels++;
        }
    }

    std::vector<AbstractionInformation> abstractions;
    std::unique_ptr<lp::LPSolver> lp_solver = utils::make_unique_ptr<lp::LPSolver>(lp_solver_type);
    vector<lp::LPVariable> variables;
    vector<lp::LPConstraint> constraints;
    double infinity = lp_solver->get_infinity();
    int num_abstract_states = 0;
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

        AbstractionInformation abstraction_info(move(mas_representation));
        const TransitionSystem &ts = fts.get_transition_system(index);
        num_abstract_states += ts.get_size();
        create_abstraction_variables(
            variables, infinity, abstraction_info, ts.get_size(), num_labels);
        create_abstraction_constraints(
            variables, constraints, infinity, abstraction_info, ts, contiguous_label_mapping);
        abstractions.push_back(move(abstraction_info));
    }
    create_global_constraints(constraints, labels, contiguous_label_mapping, abstractions);

    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Abstract states in projections: " << num_abstract_states << endl;
        cout << "LP variables: " << variables.size() << endl;
        cout << "LP constraints: " << constraints.size() << endl;
        cout << "LP peak memory before load: " << utils::get_peak_memory_in_kb() << endl;
    }

    lp_solver->load_problem(lp::LPObjectiveSense::MAXIMIZE, variables, constraints);
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "LP peak memory before solve: " << utils::get_peak_memory_in_kb() << endl;
    }

    return utils::make_unique_ptr<OptimalCostPartitioning>(move(abstractions), move(lp_solver));
}

static shared_ptr<OptimalCostPartitioningFactory>_parse(OptionParser &parser) {
    lp::add_lp_solver_option_to_parser(parser);
    parser.add_option<bool>(
        "allow_negative_costs",
        "general cost partitioning allows positive and negative operator costs. "
        "Set to false for non-negative cost partitioning.",
        "true");
    parser.add_option<bool>(
        "efficient_cp",
        "use only one constraint per label group rather than per label",
        "true");

    Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<OptimalCostPartitioningFactory>(opts);
}

static Plugin<CostPartitioningFactory> _plugin("ocp", _parse);
}
