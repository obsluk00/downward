#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H

#include "order_generator.h"

namespace merge_and_shrink {
class OrderGeneratorRandom : public OrderGenerator {
    std::vector<int> random_order;
public:
    explicit OrderGeneratorRandom(const options::Options &opts);

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
