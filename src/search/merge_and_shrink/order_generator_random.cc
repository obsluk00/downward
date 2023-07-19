#include "order_generator_random.h"

#include "cost_partitioning.h"
#include "utils.h"

#include "../task_proxy.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace merge_and_shrink {
OrderGeneratorRandom::OrderGeneratorRandom(const plugins::Options &opts) :
    OrderGenerator(opts) {
}

// TODO: this could store a random order via precompute_info and shuffle it
// when asked for an order.

Order OrderGeneratorRandom::compute_order(
    const Abstractions &abstractions,
    const vector<int> &,
    utils::LogProxy &,
    const vector<int> &) {
    Order order = get_default_order(abstractions.size());
    rng->shuffle(order);
    return order;
}


class OrderGeneratorRandomFeature : public plugins::TypedFeature<OrderGenerator, OrderGeneratorRandom> {
public:
    OrderGeneratorRandomFeature() : TypedFeature("random_orders") {
        document_synopsis(
            "Random orders: Shuffle abstractions randomly.");
        add_common_order_generator_options(*this);
    }
};

static plugins::FeaturePlugin<OrderGeneratorRandomFeature> _plugin;
}
