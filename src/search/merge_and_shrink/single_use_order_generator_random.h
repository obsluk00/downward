#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_RANDOM_H

#include "single_use_order_generator.h"

namespace merge_and_shrink {
class SingleUseOrderGeneratorRandom : public SingleUseOrderGenerator {
    const bool fixed_order;
    std::vector<int> factor_order;
public:
    explicit SingleUseOrderGeneratorRandom(const options::Options &opts);

    virtual void initialize(const TaskProxy &task_proxy) override;

    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::Verbosity verbosity) override;
};
}

#endif
