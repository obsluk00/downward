#include "order_generator_random.h"

#include "cost_partitioning.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace merge_and_shrink {
MASOrderGeneratorRandom::MASOrderGeneratorRandom(const Options &opts) :
    MASOrderGenerator(opts),
    fixed_order(opts.get<bool>("fixed_order")) {
}

void MASOrderGeneratorRandom::initialize(const TaskProxy &task_proxy) {
    utils::Log() << "Initialize random order generator" << endl;
    int num_variables = task_proxy.get_variables().size();
    int max_transition_system_count = num_variables * 2 - 1;
    factor_order.reserve(max_transition_system_count);

    if (fixed_order) {
        factor_order.resize(max_transition_system_count);
        iota(factor_order.begin(), factor_order.end(), 0);
        rng->shuffle(factor_order);
    }
}

Order MASOrderGeneratorRandom::compute_order_for_state(
    const Abstractions &abstractions,
    const vector<int> &,
    const std::vector<std::vector<int>> &,
    const std::vector<std::vector<int>> &,
    bool) {
    vector<int> abstraction_order;
    abstraction_order.reserve(abstractions.size());
    if (!fixed_order) {
        assert(factor_order.empty());
        abstraction_order.resize(abstractions.size());
        iota(abstraction_order.begin(), abstraction_order.end(), 0);
        rng->shuffle(abstraction_order);
        return abstraction_order;
    } else {
        assert(!factor_order.empty());
        for (int abs_id : factor_order) {
            int index = -1;
            for (size_t i = 0; i < abstractions.size(); ++i) {
                if (abs_id == abstractions[i]->fts_index) {
                    index = i;
                    break;
                }
            }
            if (index != -1) {
                abstraction_order.push_back(index);
            }
        }
    }
    assert(abstraction_order.size() == abstractions.size());
    return abstraction_order;
}


static shared_ptr<MASOrderGenerator> _parse_greedy(OptionParser &parser) {
    parser.add_option<bool>(
        "fixed_order",
        "If true, compute a single fixed random order used for all calls to "
        "compute_order_for_abstractions",
        "true");
    add_common_order_generator_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<MASOrderGeneratorRandom>(opts);
}

static Plugin<MASOrderGenerator> _plugin_greedy("mas_random_orders", _parse_greedy);
}
