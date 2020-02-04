#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_MAS_H

#include "single_use_order_generator.h"

namespace merge_and_shrink {
class SingleUseOrderGeneratorMAS : public SingleUseOrderGenerator {
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
    explicit SingleUseOrderGeneratorMAS(const options::Options &opts);

    virtual void initialize(const TaskProxy &task_proxy) override;

    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::Verbosity verbosity) override;
};
}

#endif
