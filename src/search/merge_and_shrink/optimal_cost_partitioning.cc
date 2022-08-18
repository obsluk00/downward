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
int AbstractionInformation::get_local_label_cost_variable(int label_no) const {
    assert(label_no >= 0);
    return local_label_cost_offset + label_no;
}

int AbstractionInformation::get_state_cost_variable(int state_id) const {
    return state_cost_offset + state_id;
}

OptimalCostPartitioning::OptimalCostPartitioning(
    vector<AbstractionInformation> &&abstraction_infos,
    unique_ptr<lp::LPSolver> lp_solver)
    : CostPartitioning(),
      abstraction_infos(move(abstraction_infos)),
      lp_solver(move(lp_solver)) {
}

bool OptimalCostPartitioning::set_current_state(const State &state) {
    // Change objective to maximize heuristic value of initial states in all projections.
    for (AbstractionInformation &abstraction_info : abstraction_infos) {
        // First check if the state is a dead end.
        int abstract_state = abstraction_info.abstraction_function->get_value(state);
        if (abstract_state == PRUNED_STATE) {
            return false;
        }

        // Unset previous state.
        lp_solver->set_objective_coefficient(abstraction_info.variable_in_objective, 0);

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
        log << "LP solve time: " << solve_time << endl;
        log << "LP peak memory after solve: " << utils::get_peak_memory_in_kb() << endl;

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

int OptimalCostPartitioning::get_number_of_abstractions() const {
    return abstraction_infos.size();
}

OptimalCostPartitioningFactory::OptimalCostPartitioningFactory(
    const Options &opts)
    : CostPartitioningFactory(),
      lp_solver_type(opts.get<lp::LPSolverType>("lpsolver")),
      allow_negative_costs(opts.get<bool>("allow_negative_costs")),
      efficient_cp(opts.get<bool>("efficient_cp")) {
}

void OptimalCostPartitioningFactory::create_abstraction_variables(
    named_vector::NamedVector<lp::LPVariable> &variables,
    double infinity,
    AbstractionInformation &abstraction_info,
    int num_states,
    int num_labels) {
    // Create variables for local label cost.
    abstraction_info.local_label_cost_offset = variables.size();
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
    named_vector::NamedVector<lp::LPVariable> &variables,
    named_vector::NamedVector<lp::LPConstraint> &constraints,
    double infinity,
    const AbstractionInformation &abstraction_info,
    const TransitionSystem &ts,
    const vector<int> &contiguous_label_mapping,
    utils::LogProxy &log) const {
    // Add constraints: H_alpha(g) <= 0 (as variable lower bounds) for all abstract goals g.
    for (int state = 0; state < ts.get_size(); ++state) {
        if (ts.is_goal_state(state)) {
            int goal_var = abstraction_info.get_state_cost_variable(state);
            variables[goal_var].upper_bound = 0;
        }
    }

    for (GroupAndTransitions gat : ts) {
        if (log.is_at_least_debug()) {
            log << "Label group: [";
            for (int label : gat.label_group) {
                log << label << " ";
            }
            log << "]" << endl;
        }
        if (efficient_cp) {
            // Label group var is the same for the group.
            int some_label_no = *gat.label_group.begin();
            int group_id = ts.get_label_equivalence_relation().get_group_id(some_label_no);
            int group_var = abstraction_info.get_local_label_cost_variable(
                contiguous_label_mapping[group_id]);

            bool have_set_lower_bound = false;
            for (const Transition &transition : gat.transitions) {
                if (transition.src != transition.target) {
                    // Create constraints for state-changing transitions.
                    int source_var = abstraction_info.get_state_cost_variable(transition.src);
                    int target_var = abstraction_info.get_state_cost_variable(transition.target);

                    // Add constraint: H_alpha(s) <= H_alpha(s') + C_alpha(l)
                    lp::LPConstraint constraint(0, infinity);
                    constraint.insert(source_var, -1);
                    constraint.insert(group_var, 1);
                    constraint.insert(target_var, 1);
                    constraints.push_back(constraint);
                    if (log.is_at_least_debug()) {
                        log << "adding transition constraint: " << source_var
                                     << " <= " << target_var << " + " << group_var
                                     << endl;
                    }
                } else {
                    if (!have_set_lower_bound) {
                        /*
                          Self loops are a special case of transitions that can be treated more
                          efficiently, because the variables H_alpha(s) and H_alpha(s') cancel out.
                        */
                        variables[group_var].lower_bound = 0;
                        have_set_lower_bound = true;
                        if (log.is_at_least_debug()) {
                            log << "lower-bounding group: " << group_var << endl;
                        }
                    }
                }
            }
        } else {
            bool have_set_lower_bound = false;
            for (const Transition &transition : gat.transitions) {
                if (transition.src != transition.target) {
                    // Create constraints for state-changing transitions.
                    int source_var = abstraction_info.get_state_cost_variable(transition.src);
                    int target_var = abstraction_info.get_state_cost_variable(transition.target);
                    for (int label_no : gat.label_group) {
                        int label_var = abstraction_info.get_local_label_cost_variable(
                            contiguous_label_mapping[label_no]);

                        // Add constraint: H_alpha(s) <= H_alpha(s') + C_alpha(l)
                        lp::LPConstraint constraint(0, infinity);
                        constraint.insert(source_var, -1);
                        constraint.insert(label_var, 1);
                        constraint.insert(target_var, 1);
                        constraints.push_back(constraint);
                        if (log.is_at_least_debug()) {
                            log << "adding transition constraint: " << source_var
                                         << " <= " << target_var << " + " << label_var
                                         << endl;
                        }
                    }
                } else {
                    if (!have_set_lower_bound) {
                        /*
                          Self loops are a special case of transitions that can be treated more
                          efficiently, because the variables H_alpha(s) and H_alpha(s') cancel out.
                        */
                        for (int label_no : gat.label_group) {
                            int label_var = abstraction_info.get_local_label_cost_variable(
                                contiguous_label_mapping[label_no]);
                            variables[label_var].lower_bound = 0;
                            if (log.is_at_least_debug()) {
                                log << "lower-bounding label: " << label_var << endl;
                            }
                        }
                        have_set_lower_bound = true;
                    }
                }
            }
        }
    }
}

void OptimalCostPartitioningFactory::create_global_constraints(
    double infinity,
    named_vector::NamedVector<lp::LPConstraint> &constraints,
    const vector<int> &label_costs,
    vector<unique_ptr<Abstraction>> &abstractions,
    const vector<vector<int>> &abs_to_contiguous_label_group_mapping,
    const vector<AbstractionInformation> &abstraction_infos,
    utils::LogProxy &log) const {
    for (size_t label_no = 0; label_no < label_costs.size(); ++label_no) {
        int label_cost = label_costs[label_no];
        if (label_cost != -1) {
            // Add constraint: sum_alpha Cost_alpha(l) <= cost(l)
            lp::LPConstraint constraint(-infinity, label_cost);
            if (log.is_at_least_debug()) {
                log << "adding global constraint for label " << label_no << ": ";
            }
            if (efficient_cp) {
                for (size_t i = 0; i < abstraction_infos.size(); ++i) {
                    const TransitionSystem &ts = *abstractions[i]->transition_system;
                    int group_id;
                    if (abstractions[i]->label_mapping.empty()) {
                        group_id = ts.get_label_equivalence_relation().get_group_id(label_no);
                    } else {
                        int abs_label = abstractions[i]->label_mapping[label_no];
                        group_id = ts.get_label_equivalence_relation().get_group_id(abs_label);
                    }
                    int group_var = abstraction_infos[i].get_local_label_cost_variable(
                        abs_to_contiguous_label_group_mapping[i][group_id]);
                    constraint.insert(group_var, 1);
                    if (log.is_at_least_debug()) {
                        log << group_var << " + ";
                    }
                }
            } else {
                for (size_t i = 0; i < abstraction_infos.size(); ++i) {
                    int label_var;
                    if (abstractions[i]->label_mapping.empty()) {
                        label_var = abstraction_infos[i].get_local_label_cost_variable(
                            abs_to_contiguous_label_group_mapping[i][label_no]);
                    } else {
                        int abs_label = abstractions[i]->label_mapping[label_no];
                        label_var = abstraction_infos[i].get_local_label_cost_variable(
                            abs_to_contiguous_label_group_mapping[i][abs_label]);
                    }
                    constraint.insert(label_var, 1);
                    if (log.is_at_least_debug()) {
                        log << label_var << " + ";
                    }
                }
            }
            constraints.push_back(constraint);
            if (log.is_at_least_debug()) {
                log << " <= " << label_cost << endl;
            }
        }
    }
}

int compute_contiguous_label_group_mapping(
    const Abstraction &abstraction, vector<int> &contiguous_label_group_mapping) {
    const TransitionSystem &ts = *abstraction.transition_system;
    const LabelEquivalenceRelation &label_equiv_rel = ts.get_label_equivalence_relation();
    int largest_group_id = label_equiv_rel.get_size();
    contiguous_label_group_mapping.resize(largest_group_id, -1);
    int num_groups = 0;
    for (int group_id = 0; group_id < largest_group_id; ++group_id) {
        if (!label_equiv_rel.is_empty_group(group_id)) {
            contiguous_label_group_mapping[group_id] = num_groups++;
        }
    }
    return num_groups;
}

int compute_contiguous_label_mapping(
    const Abstraction &abstraction, const vector<int> &label_costs, vector<int> &contiguous_label_mapping) {
    if (abstraction.label_mapping.empty()) {
        contiguous_label_mapping.resize(label_costs.size(), -1);
        int num_labels = 0;
        for (size_t label_no = 0; label_no < label_costs.size(); ++label_no) {
            if (label_costs[label_no] != -1) {
                contiguous_label_mapping[label_no] = num_labels++;
            }
        }
        return num_labels;
    } else {
        const vector<int> &label_mapping = abstraction.label_mapping;
        set<int> labels(label_mapping.begin(), label_mapping.end());
        int largest_label_no = *--labels.end();
        contiguous_label_mapping.resize(largest_label_no + 1, -1);
        int num_labels = 0;
        for (int label_no : labels) {
            contiguous_label_mapping[label_no] = num_labels++;
        }
        return num_labels;
    }
}

unique_ptr<CostPartitioning> OptimalCostPartitioningFactory::generate(
    vector<int> &&label_costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::LogProxy &log) {
    if (log.is_at_least_debug()) {
        log << "Computing OCP over M&S abstractions..." << endl;
        log << "LP peak memory before construct: " << utils::get_peak_memory_in_kb() << endl;
        log << "Original label costs: " << label_costs << endl;
    }

    // Used for labels or label groups depending on whether efficient_cp is set or not.
    vector<vector<int>> abs_to_contiguous_label_group_mapping;
    vector<int> abs_to_num_label_groups;
    abs_to_contiguous_label_group_mapping.reserve(abstractions.size());
    abs_to_num_label_groups.reserve(abstractions.size());
    for (const auto &abstraction : abstractions) {
        vector<int> contiguous_label_mapping;
        int num_labels;
        if (efficient_cp) {
            num_labels = compute_contiguous_label_group_mapping(
                *abstraction, contiguous_label_mapping);
        } else {
            /*
              NOTE: for interleaved OCPs, we would need to compute this only
              once because the mapping is the same for all abstractions. This
              used to be case before and most likely is the culprit for the
              performance degradation of this non-efficient encoding:
              https://ai.dmi.unibas.ch/_tmp_files/sieverss/2020-01-30-sccdfp-ocp-comparison.html
              However, because the efficient encoding benefits a lot from the
              refactoring that came together with this change, and because
              using the efficient encoding makes more sense anyway, we don't
              bother changing this implementation again.
            */
            num_labels = compute_contiguous_label_mapping(
                *abstraction, label_costs, contiguous_label_mapping);
        }
        abs_to_contiguous_label_group_mapping.push_back(move(contiguous_label_mapping));
        abs_to_num_label_groups.push_back(num_labels);
    }

    vector<AbstractionInformation> abstraction_infos;
    abstraction_infos.reserve(abstractions.size());
    unique_ptr<lp::LPSolver> lp_solver = utils::make_unique_ptr<lp::LPSolver>(lp_solver_type);
    named_vector::NamedVector<lp::LPVariable> variables;
    named_vector::NamedVector<lp::LPConstraint> constraints;
    double infinity = lp_solver->get_infinity();
    int num_abstract_states = 0;
    for (size_t i = 0; i < abstractions.size(); ++i) {
        if (log.is_at_least_debug()) {
            log << endl;
            log << "Building LP for abstraction " << i << endl;
            log << "Label mapping (empty if interleaved): "
                         << abstractions[i]->label_mapping << endl;
        }
        AbstractionInformation abstraction_info(
            move(abstractions[i]->merge_and_shrink_representation));
        const TransitionSystem &ts = *abstractions[i]->transition_system;
        num_abstract_states += ts.get_size();
        if (log.is_at_least_debug()) {
            log << "Number of states: " << num_abstract_states << endl;
            log << "Number of labels/label groups: " << abs_to_num_label_groups[i] << endl;
        }
        create_abstraction_variables(
            variables, infinity, abstraction_info, ts.get_size(),
            abs_to_num_label_groups[i]);
        create_abstraction_constraints(
            variables, constraints, infinity, abstraction_info, ts,
            abs_to_contiguous_label_group_mapping[i], log);
        abstraction_infos.push_back(move(abstraction_info));
    }
    create_global_constraints(
        infinity, constraints, label_costs, abstractions,
        abs_to_contiguous_label_group_mapping, abstraction_infos, log);

    for (auto &abs : abstractions) {
        if (!abs->label_mapping.empty()) {
            delete abs->transition_system;
            abs->transition_system = nullptr;
        }
    }

    if (log.is_at_least_debug()) {
        log << "Abstract states in abstractions: " << num_abstract_states << endl;
        log << "LP variables: " << variables.size() << endl;
        log << "LP constraints: " << constraints.size() << endl;
        log << "LP peak memory before load: " << utils::get_peak_memory_in_kb() << endl;
    }

    lp::LinearProgram lp(lp::LPObjectiveSense::MAXIMIZE, move(variables), move(constraints), lp_solver->get_infinity());
    lp_solver->load_problem(lp);
    if (log.is_at_least_debug()) {
        log << "LP peak memory after load: " << utils::get_peak_memory_in_kb() << endl;
    }

    return utils::make_unique_ptr<OptimalCostPartitioning>(move(abstraction_infos), move(lp_solver));
}

static shared_ptr<OptimalCostPartitioningFactory>_parse(OptionParser &parser) {
    lp::add_lp_solver_option_to_parser(parser);
    parser.add_option<bool>(
        "allow_negative_costs",
        "general cost partitioning allows positive and negative label costs. "
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
