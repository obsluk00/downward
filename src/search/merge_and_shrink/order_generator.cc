#include "order_generator.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../utils/rng_options.h"

#include <numeric>

using namespace std;

namespace merge_and_shrink {
Order get_default_order(int num_abstractions) {
    vector<int> indices(num_abstractions);
    iota(indices.begin(), indices.end(), 0);
    return indices;
}

OrderGenerator::OrderGenerator(const plugins::Options &opts)
    : rng(utils::parse_rng_from_options(opts)) {
}

void add_common_order_generator_options(plugins::Feature &feature) {
    utils::add_rng_options(feature);
}

static class OrderGeneratorCategoryPlugin : public plugins::TypedCategoryPlugin<OrderGenerator> {
public:
    OrderGeneratorCategoryPlugin() : TypedCategoryPlugin("OrderGenerator") {
        document_synopsis(
            "Generate heuristic orders.");
    }
}
_category_plugin;
}
