#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_H

#include "types.h"

#include <vector>

class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class RandomNumberGenerator;
class LogProxy;
}

namespace merge_and_shrink {
extern Order get_default_order(int num_abstractions);

class OrderGenerator {
protected:
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
public:
    explicit OrderGenerator(const options::Options &opts);
    virtual ~OrderGenerator() = default;

    // This is a HACK for greedy order generator: they need to precompute
    // information for a current set of abstractions for being reusable
    // efficiently. We preferred to not do this via the previous initialize
    // method because we might use a generator for different sets of
    // abstractions.
    virtual void clear_internal_state() = 0;
    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::LogProxy &log,
        const std::vector<int> &abstract_state_ids = std::vector<int>()) = 0;
};

extern void add_common_order_generator_options(options::OptionParser &parser);
}

#endif
