#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H

#include "order_generator.h"

namespace merge_and_shrink {
class MASOrderGeneratorMAS : public MASOrderGenerator {
    enum class AtomicTSOrder {
        REVERSE_LEVEL, // regular FD variable order
        LEVEL, // reverse of above
        RANDOM
    };
    const AtomicTSOrder atomic_ts_order;
    enum class ProductTSOrder {
        OLD_TO_NEW,
        NEW_TO_OLD,
        RANDOM
    };
    const ProductTSOrder product_ts_order;
    const bool atomic_before_product;
    std::vector<int> factor_order;
public:
    explicit MASOrderGeneratorMAS(const options::Options &opts);

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
