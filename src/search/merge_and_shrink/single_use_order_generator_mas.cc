#include "single_use_order_generator_mas.h"

#include "cost_partitioning.h"
#include "utils.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../utils/logging.h"
#include "../utils/rng.h"

using namespace std;

namespace merge_and_shrink {
SingleUseOrderGeneratorMAS::SingleUseOrderGeneratorMAS(const Options &opts) :
    SingleUseOrderGenerator(opts),
    atomic_ts_order(AtomicTSOrder(opts.get_enum("atomic_ts_order"))),
    product_ts_order(ProductTSOrder(opts.get_enum("product_ts_order"))),
    atomic_before_product(opts.get<bool>("atomic_before_product")) {
}

void SingleUseOrderGeneratorMAS::initialize(const TaskProxy &task_proxy) {
    int num_variables = task_proxy.get_variables().size();
    int max_transition_system_count = num_variables * 2 - 1;
    factor_order.reserve(max_transition_system_count);

    // Compute the order in which atomic transition systems are considered
    vector<int> atomic_tso(num_variables);
    iota(atomic_tso.begin(), atomic_tso.end(), 0);
    if (atomic_ts_order == AtomicTSOrder::LEVEL) {
        reverse(atomic_tso.begin(), atomic_tso.end());
    } else if (atomic_ts_order == AtomicTSOrder::RANDOM) {
        rng->shuffle(atomic_tso);
    }

    // Compute the order in which product transition systems are considered
    vector<int> product_tso(max_transition_system_count - num_variables);
    iota(product_tso.begin(), product_tso.end(), num_variables);
    if (product_ts_order == ProductTSOrder::NEW_TO_OLD) {
        reverse(product_tso.begin(), product_tso.end());
    } else if (product_ts_order == ProductTSOrder::RANDOM) {
        rng->shuffle(product_tso);
    }

    // Put the orders in the correct order
    if (atomic_before_product) {
        factor_order.insert(
            factor_order.end(), atomic_tso.begin(), atomic_tso.end());
        factor_order.insert(
            factor_order.end(), product_tso.begin(), product_tso.end());
    } else {
        factor_order.insert(
            factor_order.end(), product_tso.begin(), product_tso.end());
        factor_order.insert(
            factor_order.end(), atomic_tso.begin(), atomic_tso.end());
    }
}

Order SingleUseOrderGeneratorMAS::compute_order_for_state(
    const Abstractions &abstractions,
    const vector<int> &,
    utils::Verbosity) {
    vector<int> abstraction_order;
    abstraction_order.reserve(abstractions.size());
    for (int abs_id : factor_order) {
        int index = -1;
        for (size_t i = 0; i < abstractions.size(); ++i) {
            // TODO: this assumes that fts indices are unique! If we used
            // offline CP and used the "same" abstraction from an iteration
            // several times, this would not hold!
            if (abs_id == abstractions[i]->fts_index) {
                index = i;
                break;
            }
        }
        if (index != -1) {
            abstraction_order.push_back(index);
        }
    }
    assert(abstraction_order.size() == abstractions.size());
    return abstraction_order;
}


static shared_ptr<SingleUseOrderGenerator> _parse_greedy(OptionParser &parser) {
    vector<string> atomic_ts_order;
    vector<string> atomic_ts_order_documentation;
    atomic_ts_order.push_back("reverse_level");
    atomic_ts_order_documentation.push_back(
        "the variable order of Fast Downward");
    atomic_ts_order.push_back("level");
    atomic_ts_order_documentation.push_back("opposite of reverse_level");
    atomic_ts_order.push_back("random");
    atomic_ts_order_documentation.push_back("a randomized order");
    parser.add_enum_option(
        "atomic_ts_order",
        atomic_ts_order,
        "The order in which atomic transition systems are considered when "
        "considering pairs of potential merges.",
        "reverse_level",
        atomic_ts_order_documentation);

    vector<string> product_ts_order;
    vector<string> product_ts_order_documentation;
    product_ts_order.push_back("old_to_new");
    product_ts_order_documentation.push_back(
        "consider composite transition systems from most recent to oldest, "
        "that is in decreasing index order");
    product_ts_order.push_back("new_to_old");
    product_ts_order_documentation.push_back("opposite of old_to_new");
    product_ts_order.push_back("random");
    product_ts_order_documentation.push_back("a randomized order");
    parser.add_enum_option(
        "product_ts_order",
        product_ts_order,
        "The order in which product transition systems are considered when "
        "considering pairs of potential merges.",
        "new_to_old",
        product_ts_order_documentation);

    parser.add_option<bool>(
        "atomic_before_product",
        "Consider atomic transition systems before composite ones iff true.",
        "false");

    add_common_order_generator_options(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<SingleUseOrderGeneratorMAS>(opts);
}

static Plugin<SingleUseOrderGenerator> _plugin_greedy("mas_fixed_orders", _parse_greedy);
}
