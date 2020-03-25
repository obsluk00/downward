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
enum class Verbosity;
}

namespace merge_and_shrink {
extern Order get_default_order(int num_abstractions);

class OrderGenerator {
protected:
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
public:
    explicit OrderGenerator(const options::Options &opts);
    virtual ~OrderGenerator() = default;

    virtual void initialize(const TaskProxy &task_proxy) = 0;
    // This is a HACK for greedy order generator: they need to precompute
    // information for a current set of abstractions for being reusable
    // efficiently. The initialize method, on the other hand, is a one-time
    // initialization that is *not* cleared.
    virtual void clear_internal_state() = 0;
    virtual Order compute_order(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        utils::Verbosity verbosity,
        const std::vector<int> &abstract_state_ids = std::vector<int>()) = 0;
};

extern void add_common_order_generator_options(options::OptionParser &parser);
}

#endif
