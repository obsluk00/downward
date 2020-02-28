#include "order_generator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/rng_options.h"

#include <numeric>

using namespace std;

namespace merge_and_shrink {
Order get_default_order(int num_abstractions) {
    vector<int> indices(num_abstractions);
    iota(indices.begin(), indices.end(), 0);
    return indices;
}

OrderGenerator::OrderGenerator(const options::Options &opts)
    : rng(utils::parse_rng_from_options(opts)) {
}

void add_common_order_generator_options(OptionParser &parser) {
    utils::add_rng_options(parser);
}

static PluginTypePlugin<OrderGenerator> _type_plugin(
    "OrderGenerator",
    "Generate heuristic orders.");
}
