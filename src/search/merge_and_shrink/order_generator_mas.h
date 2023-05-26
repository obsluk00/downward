#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H

#include "order_generator.h"

namespace merge_and_shrink {
enum class AtomicTSOrder {
    REVERSE_LEVEL, // regular FD variable order
    LEVEL, // reverse of above
    RANDOM
};

enum class ProductTSOrder {
    OLD_TO_NEW,
    NEW_TO_OLD,
    RANDOM
};
class OrderGeneratorMAS : public OrderGenerator {
    const AtomicTSOrder atomic_ts_order;
    const ProductTSOrder product_ts_order;
    const bool atomic_before_product;
public:
    explicit OrderGeneratorMAS(const plugins::Options &opts);

    virtual void clear_internal_state() override {}
    /*
      This method assumes that abstractions are ordered by the generation
      time, i.e., oldest abstractions are ordered first. In particular, atomic
      abstractions are ordered according to the regular variable order, and
      any product abstractions in the order they were generated.
    */
    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::LogProxy &log,
        const std::vector<int> &abstract_state_ids) override;
};
}

#endif
