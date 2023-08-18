#include "order_generator_mas.h"

#include "cost_partitioning.h"
#include "transition_system.h"
#include "utils.h"

#include "../task_proxy.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"
#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace merge_and_shrink {
OrderGeneratorMAS::OrderGeneratorMAS(const plugins::Options &opts) :
    OrderGenerator(opts),
    atomic_ts_order(opts.get<AtomicTSOrder>("atomic_ts_order")),
    product_ts_order(opts.get<ProductTSOrder>("product_ts_order")),
    atomic_before_product(opts.get<bool>("atomic_before_product")) {
}

Order OrderGeneratorMAS::compute_order(
    const Abstractions &abstractions,
    const vector<int> &,
    utils::LogProxy &,
    const vector<int> &) {
    // Collect atomic and product abstractions.
    Order atomic_order;
    Order product_order;
    for (size_t i = 0; i < abstractions.size(); ++i) {
        bool is_atomic = abstractions[i]->transition_system->get_incorporated_variables().size() == 1;
        if (is_atomic) {
            atomic_order.push_back(i);
        } else {
            product_order.push_back(i);
        }
    }

    // Compute the order of atomic abstractions. The default order is reverse
    // level and hence we don't have to change it in that case.
    if (atomic_ts_order == AtomicTSOrder::LEVEL) {
        reverse(atomic_order.begin(), atomic_order.end());
    } else if (atomic_ts_order == AtomicTSOrder::RANDOM) {
        rng->shuffle(atomic_order);
    }

    // Compute the order of product abstractions. The default order is old
    // to new and hence we don't have to change it in that case.
    if (product_ts_order == ProductTSOrder::NEW_TO_OLD) {
        reverse(product_order.begin(), product_order.end());
    } else if (product_ts_order == ProductTSOrder::RANDOM) {
        rng->shuffle(product_order);
    }

    // Combine atomic and product orders in the correct order.
    Order order;
    order.reserve(abstractions.size());
    if (atomic_before_product) {
        order.insert(order.end(), atomic_order.begin(), atomic_order.end());
        order.insert(order.end(), product_order.begin(), product_order.end());
    } else {
        order.insert(order.end(), product_order.begin(), product_order.end());
        order.insert(order.end(), atomic_order.begin(), atomic_order.end());
    }
    assert(order.size() == abstractions.size());
    return order;
}


class OrderGeneratorMASFeature : public plugins::TypedFeature<OrderGenerator, OrderGeneratorMAS> {
public:
    OrderGeneratorMASFeature() : TypedFeature("fixed_orders") {
        add_option<AtomicTSOrder>(
            "atomic_ts_order",
            "The order in which atomic transition systems are considered when "
            "considering pairs of potential merges.",
            "reverse_level");
        add_option<ProductTSOrder>(
            "product_ts_order",
            "The order in which product transition systems are considered when "
            "considering pairs of potential merges.",
            "new_to_old");
        add_option<bool>(
            "atomic_before_product",
            "Consider atomic transition systems before product ones iff true.",
            "false");

        add_common_order_generator_options(*this);
    }
};

static plugins::FeaturePlugin<OrderGeneratorMASFeature> _plugin;
}