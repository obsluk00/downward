#ifndef MERGE_AND_SHRINK_OPTIMAL_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_OPTIMAL_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../option_parser.h"

#include "../lp/lp_solver.h"

#include <vector>

namespace merge_and_shrink {
class FactoredTransitionSystem;
class LabelGroup;
class Labels;
class MergeAndShrinkRepresentation;
class Transition;
class TransitionSystem;

struct AbstractionInformation {
    int state_cost_offset;
    int local_op_cost_offset;
    int variable_in_objective;
    std::unique_ptr<MergeAndShrinkRepresentation> abstraction_function;

    int get_local_op_cost_variable(int op_id) const;
    int get_state_cost_variable(int state_id) const;

    AbstractionInformation(std::unique_ptr<MergeAndShrinkRepresentation> abstraction_function)
        : state_cost_offset(0),
          local_op_cost_offset(0),
          variable_in_objective(0),
          abstraction_function(std::move(abstraction_function)) {
    }
};

class OptimalCostPartitioning : public CostPartitioning {
    std::vector<AbstractionInformation> abstractions;
    std::unique_ptr<lp::LPSolver> lp_solver;

    bool set_current_state(const State &state);
public:
    OptimalCostPartitioning(
        std::vector<AbstractionInformation> &&abstractions,
        std::unique_ptr<lp::LPSolver> lp_solver);
    virtual ~OptimalCostPartitioning() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_factors() const override;
};

class OptimalCostPartitioningFactory : public CostPartitioningFactory {
    lp::LPSolverType lp_solver_type;
    bool allow_negative_costs;
    // Contiguous renumbering of labels.
    std::vector<int> contiguous_label_mapping;

    void create_abstraction_variables(
        std::vector<lp::LPVariable> &variables, double infinity,
        AbstractionInformation &abstraction_info, int num_states,
        const Labels &labels);
    void create_abstraction_constraints(
        std::vector<lp::LPVariable> &variables,
        std::vector<lp::LPConstraint> &constraints,
        double infinity,
        const AbstractionInformation &abstraction_info,
        const TransitionSystem &transition_system) const;
    void create_global_constraints(
        std::vector<lp::LPConstraint> &constraints,
        const Labels &labels,
        const std::vector<AbstractionInformation> &abstractions) const;
public:
    explicit OptimalCostPartitioningFactory(const Options &opts);
    virtual ~OptimalCostPartitioningFactory() = default;
    virtual std::unique_ptr<CostPartitioning> generate(
        FactoredTransitionSystem &fts,
        utils::Verbosity verbosity,
        int unsolvable_index = -1) override;
};
}

#endif
