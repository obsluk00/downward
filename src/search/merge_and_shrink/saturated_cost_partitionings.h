#ifndef MERGE_AND_SHRINK_SATURATED_COST_PARTITIONINGS_H
#define MERGE_AND_SHRINK_SATURATED_COST_PARTITIONINGS_H

#include "cost_partitioning.h"

#include "types.h"

#include "../option_parser.h"

#include <vector>

namespace utils {
class RandomNumberGenerator;
}

namespace merge_and_shrink {
class MergeAndShrinkRepresentation;
class SingleUseOrderGenerator;

struct AbstractionInformation {
    std::vector<int> goal_distances;
    std::unique_ptr<MergeAndShrinkRepresentation> mas_representation;
};

/*
  Copy of Jendrik's CostPartitioningHeuristic.

  Compactly store cost-partitioned goal distances and compute heuristic values
  by summing the goal distances of abstract states corresponding to a given
  concrete state.

  For efficiency, users of this class need to store the abstractions and map a
  given concrete state to the corresponding abstract state IDs in all
  abstractions themselves. This allows them to compute the mapping only once
  instead of for each order.

  We call an abstraction A useful if 0 < h^A(s) < INF for at least one state s.
  To save space, we only store h values for useful abstractions.

  This class only supports retrieving finite heuristic estimates (see
  compute_heuristic() below).
*/
class CostPartitioningHeuristic {
    struct LookupTable {
        int abstraction_id;
        /* h_values[i] is the goal distance of abstract state i under the cost
           function assigned to the associated abstraction. */
        std::vector<int> h_values;

        LookupTable(int abstraction_id, std::vector<int> &&h_values)
            : abstraction_id(abstraction_id),
              h_values(move(h_values)) {
        }
    };

    std::vector<LookupTable> lookup_tables;

public:
    void add_h_values(int abstraction_id, std::vector<int> &&h_values, bool total_abstraction);

    /*
      Compute cost-partitioned heuristic value for a concrete state s. Callers
      need to precompute the abstract state IDs that s corresponds to in all
      abstractions (not only useful abstractions). The result is the sum of all
      stored heuristic values for abstract states corresponding to s.

      It is an error (guarded by an assertion) to call this method for an
      unsolvable abstract state s. Before calling this method, query
      UnsolvabilityHeuristic to see whether s is unsolvable.
    */
    int compute_heuristic(const std::vector<int> &abstract_state_ids) const;

    // Return the number of useful abstractions.
    int get_num_lookup_tables() const;

    // Return the total number of stored heuristic values.
    int get_num_heuristic_values() const;

    // See class documentation.
    void mark_useful_abstractions(std::vector<bool> &useful_abstractions) const;
};

extern CostPartitioningHeuristic compute_scp(
    const Abstractions &abstractions,
    const std::vector<int> &order,
    const std::vector<int> &costs);

// Adapted from Jendrik's MaxCostPartitioningHeuristic
class SaturatedCostPartitionings : public CostPartitioning {
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> abstraction_functions;
    std::vector<CostPartitioningHeuristic> cp_heuristics;
    const int num_original_abstractions;
public:
    SaturatedCostPartitionings(
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        std::vector<CostPartitioningHeuristic> &&cp_heuristics);
    virtual ~SaturatedCostPartitionings() = default;
    virtual int compute_value(const State &state) override;
    virtual int get_number_of_factors() const override;
};

class OrderGenerator;

// Adapted from Jendrik's CostPartitioningHeuristicCollectionGenerator
class SaturatedCostPartitioningsFactory : public CostPartitioningFactory {
    const std::shared_ptr<OrderGenerator> order_generator;
    const int max_orders;
    const double max_time;
    const bool diversify;
    const int num_samples;
    const double max_optimization_time;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
    std::shared_ptr<AbstractTask> task;

    enum class SamplingWithDeadEnds {
        None,
        Div,
        Opt,
        DivAndOpt
    };
    const SamplingWithDeadEnds sampling_with_dead_ends;

    std::unique_ptr<CostPartitioning> generate_for_order(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        const std::vector<int> order,
        utils::Verbosity verbosity) const;
public:
    explicit SaturatedCostPartitioningsFactory(const Options &opts);
    virtual ~SaturatedCostPartitioningsFactory() = default;
    virtual void initialize(const std::shared_ptr<AbstractTask> &task) override;
    virtual std::unique_ptr<CostPartitioning> generate(
        std::vector<int> &&label_costs,
        std::vector<std::unique_ptr<Abstraction>> &&abstractions,
        utils::Verbosity verbosity) override;
};
}

#endif
