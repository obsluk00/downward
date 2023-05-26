#ifndef MERGE_AND_SHRINK_OPTIMAL_COST_PARTITIONING_H
#define MERGE_AND_SHRINK_OPTIMAL_COST_PARTITIONING_H

#include "cost_partitioning.h"

#include "../algorithms/named_vector.h"
#include "../plugins/options.h"

#include "../lp/lp_solver.h"

#include <vector>

namespace merge_and_shrink {
class FactoredTransitionSystem;
class Labels;
class MergeAndShrinkRepresentation;
class Transition;
class TransitionSystem;

struct AbstractionInformation {
    int state_cost_offset;
    int local_label_cost_offset;
    int variable_in_objective;
    std::unique_ptr<MergeAndShrinkRepresentation> abstraction_function;

    int get_local_label_cost_variable(int label_no) const;
    int get_state_cost_variable(int state_id) const;

    AbstractionInformation(std::unique_ptr<MergeAndShrinkRepresentation> abstraction_function)
        : state_cost_offset(0),
          local_label_cost_offset(0),
          variable_in_objective(0),
          abstraction_function(std::move(abstraction_function)) {
    }
};

class OptimalCostPartitioning : public CostPartitioning {
    std::vector<AbstractionInformation> abstraction_infos;
    std::unique_ptr<lp::LPSolver> lp_solver;

    bool set_current_state(const State &state);
public:
    OptimalCostPartitioning(
        std::vector<AbstractionInformation> &&abstraction_infos,
        std::unique_ptr<lp::LPSolver> lp_solver);
    virtual ~OptimalCostPartitioning() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_abstractions() const override;
};

class OptimalCostPartitioningFactory : public CostPartitioningFactory {
    const lp::LPSolverType lp_solver_type;
    const bool allow_negative_costs;
    const bool efficient_cp;

    void create_abstraction_variables(
        named_vector::NamedVector<lp::LPVariable> &variables,
        double infinity,
        AbstractionInformation &abstraction_info,
        int num_states,
        int num_labels);
    void create_abstraction_constraints(
        named_vector::NamedVector<lp::LPVariable> &variables,
        named_vector::NamedVector<lp::LPConstraint> &constraints,
        double infinity,
        const AbstractionInformation &abstraction_info,
        const TransitionSystem &ts,
        const std::vector<int> &contiguous_label_mapping,
        utils::LogProxy &log) const;
    void create_global_constraints(
        double infinity,
        named_vector::NamedVector<lp::LPConstraint> &constraints,
        const std::vector<int> &label_costs,
        std::vector<std::unique_ptr<Abstraction>> &abstractions,
        const std::vector<std::vector<int>> &abs_to_contiguous_label_group_mapping,
        const std::vector<AbstractionInformation> &abstraction_infos,
        utils::LogProxy &log) const;
public:
    explicit OptimalCostPartitioningFactory(const plugins::Options &opts);
    virtual ~OptimalCostPartitioningFactory() = default;
    virtual std::unique_ptr<CostPartitioning> generate(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::LogProxy &log) override;
};
}

#endif
