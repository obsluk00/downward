#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_GREEDY_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_GREEDY_H

#include "greedy_order_utils.h"
#include "single_use_order_generator.h"

namespace options {
class Options;
}

namespace merge_and_shrink {
class SingleUseOrderGeneratorGreedy : public SingleUseOrderGenerator {
    const ScoringFunction scoring_function;

    double rate_abstraction(
        const std::unique_ptr<Abstraction> &abs,
        const std::vector<int> &h_values,
        int stolen_costs) const;
public:
    explicit SingleUseOrderGeneratorGreedy(const options::Options &opts);

    virtual void initialize(const TaskProxy &task_proxy) override;

    virtual Order compute_order_for_state(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::Verbosity verbosity) override;
};
}

#endif
