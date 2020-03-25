#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H

#include "order_generator.h"

namespace merge_and_shrink {
class OrderGeneratorRandom : public OrderGenerator {
public:
    explicit OrderGeneratorRandom(const options::Options &opts);

    virtual void clear_internal_state() override {}
    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::Verbosity verbosity,
        const std::vector<int> &abstract_state_ids) override;
};
}

#endif
