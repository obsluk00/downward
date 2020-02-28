#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_H

#include "types.h"

#include <vector>

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class RandomNumberGenerator;
}

namespace merge_and_shrink {
extern Order get_default_order(int num_abstractions);

class OrderGenerator {
protected:
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
public:
    explicit OrderGenerator(const options::Options &opts);
    virtual ~OrderGenerator() = default;

    virtual void initialize(
        const Abstractions &abstractions,
        const std::vector<int> &costs) = 0;

    virtual Order compute_order_for_state(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        const std::vector<int> &abstract_state_ids,
        bool verbose) = 0;
};

extern void add_common_order_generator_options(options::OptionParser &parser);
}

#endif
