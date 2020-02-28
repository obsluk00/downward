#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_DYNAMIC_GREEDY_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_DYNAMIC_GREEDY_H

#include "greedy_order_utils.h"
#include "order_generator.h"

namespace options {
class Options;
}

namespace merge_and_shrink {
class OrderGeneratorDynamicGreedy : public OrderGenerator {
    const ScoringFunction scoring_function;

    Order compute_dynamic_greedy_order_for_sample(
        const Abstractions &abstractions,
        const std::vector<int> &abstract_state_ids,
        std::vector<int> remaining_costs) const;

public:
    explicit OrderGeneratorDynamicGreedy(const options::Options &opts);

    virtual void initialize(
        const Abstractions &abstractions,
        const std::vector<int> &costs) override;

    virtual Order compute_order_for_state(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        const std::vector<int> &abstract_state_ids,
        bool verbose) override;
};
}

#endif
