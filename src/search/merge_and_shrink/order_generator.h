#ifndef MERGE_AND_SHRINK_ORDER_GENERATOR_H
#define MERGE_AND_SHRINK_ORDER_GENERATOR_H

#include <memory>
#include <vector>

class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

namespace utils {
class RandomNumberGenerator;
}

namespace merge_and_shrink {
struct Abstraction;
using Abstractions = std::vector<std::unique_ptr<Abstraction>>;
using Order = std::vector<int>;

class MASOrderGenerator {
protected:
    Order get_default_order(int num_abstractions) const;
    void reduce_costs(
        std::vector<int> &remaining_costs, const std::vector<int> &saturated_costs) const;
    const std::shared_ptr<utils::RandomNumberGenerator> rng;
public:
    explicit MASOrderGenerator(const options::Options &opts);
    virtual ~MASOrderGenerator() = default;

    virtual void initialize(const TaskProxy &task_proxy) = 0;

    virtual Order compute_order_for_state(
        const Abstractions &abstractions,
        const std::vector<int> &costs,
        const std::vector<std::vector<int>> &h_values_by_abstraction,
        const std::vector<std::vector<int>> &saturated_costs_by_abstraction,
        bool verbose) = 0;
};

extern void add_common_order_generator_options(options::OptionParser &parser);
}

#endif
