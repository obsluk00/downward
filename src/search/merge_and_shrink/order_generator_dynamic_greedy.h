#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_DYNAMIC_GREEDY_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_DYNAMIC_GREEDY_H

#include "greedy_order_utils.h"
#include "order_generator.h"

namespace merge_and_shrink {
class OrderGeneratorDynamicGreedy : public OrderGenerator {
    const ScoringFunction scoring_function;

    Order compute_dynamic_greedy_order_for_sample(
        const Abstractions &abstractions,
        const std::vector<int> &abstract_state_ids,
        std::vector<int> remaining_costs) const;

public:
    explicit OrderGeneratorDynamicGreedy(const plugins::Options &opts);

    virtual void clear_internal_state() override {}
    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::LogProxy &log,
        const std::vector<int> &abstract_state_ids) override;
};
}

#endif