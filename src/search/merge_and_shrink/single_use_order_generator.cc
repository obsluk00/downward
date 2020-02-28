#include "single_use_order_generator.h"

#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/rng_options.h"

#include <numeric>

using namespace std;

namespace merge_and_shrink {
SingleUseOrderGenerator::SingleUseOrderGenerator(const options::Options &opts)
    : rng(utils::parse_rng_from_options(opts)) {
}

Order SingleUseOrderGenerator::get_default_order(int num_abstractions) const {
    vector<int> indices(num_abstractions);
    iota(indices.begin(), indices.end(), 0);
    return indices;
}

void SingleUseOrderGenerator::reduce_costs(
    vector<int> &remaining_costs, const vector<int> &saturated_costs) const {
    assert(remaining_costs.size() == saturated_costs.size());
    for (size_t i = 0; i < remaining_costs.size(); ++i) {
        int &remaining = remaining_costs[i];
        int saturated = saturated_costs[i];
        assert(remaining >= 0);
        assert(saturated <= remaining);
        if (remaining == INF) {
            // Left addition: x - y = x for all values y if x is infinite.
        } else if (saturated == -INF) {
            remaining = INF;
        } else {
            assert(saturated != INF);
            remaining -= saturated;
        }
        assert(remaining >= 0);
    }
}

void add_common_single_order_generator_options(OptionParser &parser) {
    utils::add_rng_options(parser);
}

static PluginTypePlugin<SingleUseOrderGenerator> _type_plugin(
    "MasOrderGenerator",
    "Generate heuristic orders.");
}
