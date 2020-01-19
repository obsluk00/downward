#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H

#include "order_generator.h"

namespace merge_and_shrink {
class MASOrderGeneratorRandom : public MASOrderGenerator {
    const bool fixed_order;
    std::vector<int> factor_order;
public:
    explicit MASOrderGeneratorRandom(const options::Options &opts);

    virtual void initialize(const TaskProxy &task_proxy) override;

    virtual Order compute_order_for_state(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        const std::vector<std::vector<int>> &h_values_by_abstraction,
        const std::vector<std::vector<int>> &saturated_costs_by_abstraction,
        bool verbose) override;
};
}

#endif
