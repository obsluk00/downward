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
OrderGeneratorRandom::OrderGeneratorRandom(const Options &opts) :
    OrderGenerator(opts) {
}

Order OrderGeneratorRandom::compute_order(
    const Abstractions &abstractions,
    const vector<int> &,
    utils::Verbosity,
    const vector<int> &) {
    Order order = get_default_order(abstractions.size());
    rng->shuffle(order);
    return order;
}


static shared_ptr<OrderGenerator> _parse_greedy(OptionParser &parser) {
    add_common_order_generator_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<OrderGeneratorRandom>(opts);
}

static Plugin<OrderGenerator> _plugin_greedy("random_orders", _parse_greedy);
}
