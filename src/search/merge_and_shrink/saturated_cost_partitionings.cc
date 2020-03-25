#include "saturated_cost_partitionings.h"

#include "diversifier.h"
#include "merge_and_shrink_representation.h"
#include "order_generator.h"
#include "order_optimizer.h"
#include "saturated_cost_partitioning_utils.h"
#include "transition_system.h"
#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"

#include "../task_utils/sampling.h"
//#include "../task_utils/task_properties.h"
#include "../utils/collections.h"
#include "../utils/countdown_timer.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {
void CostPartitioningHeuristic::add_h_values(
    int abstraction_id, vector<int> &&h_values, bool total_abstraction) {
    if (!total_abstraction || any_of(h_values.begin(), h_values.end(), [](int h) {
                   return h > 0 && h != INF;
               })) {
        lookup_tables.emplace_back(abstraction_id, move(h_values));
    }
}

int CostPartitioningHeuristic::compute_heuristic(
    const vector<int> &abstract_state_ids) const {
    int sum_h = 0;
    for (const LookupTable &lookup_table : lookup_tables) {
        assert(utils::in_bounds(lookup_table.abstraction_id, abstract_state_ids));
        int state_id = abstract_state_ids[lookup_table.abstraction_id];
        if (state_id == merge_and_shrink::PRUNED_STATE) {
            // Silvan: this accounts for pruned states in M&S.
            sum_h = INF;
            break;
        }
        assert(utils::in_bounds(state_id, lookup_table.h_values));
        int h = lookup_table.h_values[state_id];
        assert(h >= 0 && h != INF);
        sum_h += h;
        assert(sum_h >= 0);
    }
    return sum_h;
}

int CostPartitioningHeuristic::get_num_lookup_tables() const {
    return lookup_tables.size();
}

int CostPartitioningHeuristic::get_num_heuristic_values() const {
    int num_values = 0;
    for (const auto &lookup_table : lookup_tables) {
        num_values += lookup_table.h_values.size();
    }
    return num_values;
}

void CostPartitioningHeuristic::mark_useful_abstractions(
    vector<bool> &useful_abstractions) const {
    for (const auto &lookup_table : lookup_tables) {
        assert(utils::in_bounds(lookup_table.abstraction_id, useful_abstractions));
        useful_abstractions[lookup_table.abstraction_id] = true;
    }
}

static void log_info_about_stored_lookup_tables(
    const Abstractions &abstractions,
    const vector<CostPartitioningHeuristic> &cp_heuristics) {
    int num_abstractions = abstractions.size();

    // Print statistics about the number of lookup tables.
    int num_lookup_tables = num_abstractions * cp_heuristics.size();
    int num_stored_lookup_tables = 0;
    for (const auto &cp_heuristic: cp_heuristics) {
        num_stored_lookup_tables += cp_heuristic.get_num_lookup_tables();
    }
    utils::Log() << "Stored lookup tables: " << num_stored_lookup_tables << "/"
                 << num_lookup_tables << " = "
                 << num_stored_lookup_tables / static_cast<double>(num_lookup_tables)
                 << endl;

    // Print statistics about the number of stored values.
    int num_stored_values = 0;
    for (const auto &cp_heuristic : cp_heuristics) {
        num_stored_values += cp_heuristic.get_num_heuristic_values();
    }
    int num_total_values = 0;
    for (const auto &abstraction : abstractions) {
        num_total_values += abstraction->transition_system->get_size();
    }
    num_total_values *= cp_heuristics.size();
    utils::Log() << "Stored values: " << num_stored_values << "/"
                 << num_total_values << " = "
                 << num_stored_values / static_cast<double>(num_total_values) << endl;
}

static vector<unique_ptr<MergeAndShrinkRepresentation>>
extract_abstraction_functions_from_useful_abstractions(
    const vector<CostPartitioningHeuristic> &cp_heuristics,
    Abstractions &abstractions) {
    int num_abstractions = abstractions.size();

    // Collect IDs of useful abstractions.
    vector<bool> useful_abstractions(num_abstractions, false);
    for (const auto &cp_heuristic : cp_heuristics) {
        cp_heuristic.mark_useful_abstractions(useful_abstractions);
    }

    vector<unique_ptr<MergeAndShrinkRepresentation>> abstraction_functions;
    abstraction_functions.reserve(num_abstractions);
    for (int i = 0; i < num_abstractions; ++i) {
        if (useful_abstractions[i]) {
            abstraction_functions.push_back(
                abstractions[i]->extract_abstraction_function());
        } else {
            abstraction_functions.push_back(nullptr);
        }
    }
    return abstraction_functions;
}

SaturatedCostPartitionings::SaturatedCostPartitionings(
    std::vector<std::unique_ptr<Abstraction>> &&abstractions,
    std::vector<CostPartitioningHeuristic> &&cp_heuristics_)
    : CostPartitioning(),
      cp_heuristics(move(cp_heuristics_)),
      num_original_abstractions(abstractions.size()) {
      log_info_about_stored_lookup_tables(abstractions, cp_heuristics);

      // We only need abstraction functions during search and no transition systems.
      abstraction_functions = extract_abstraction_functions_from_useful_abstractions(
          cp_heuristics, abstractions);

      int num_abstractions = abstractions.size();
      int num_useful_abstractions = abstraction_functions.size();
      utils::Log() << "Useful abstractions: " << num_useful_abstractions << "/"
                   << num_abstractions << " = "
                   << static_cast<double>(num_useful_abstractions) / num_abstractions
                   << endl;
}

static std::vector<int> get_abstract_state_ids(
    const vector<unique_ptr<MergeAndShrinkRepresentation>> &abstractions, const State &state) {
    std::vector<int> abstract_state_ids;
    abstract_state_ids.reserve(abstractions.size());
    for (auto &abstraction : abstractions) {
        if (abstraction) {
            // Only add local state IDs for useful abstractions.
            abstract_state_ids.push_back(abstraction->get_value(state));
        } else {
            // Add dummy value if abstraction will never be used.
            abstract_state_ids.push_back(-1);
        }
    }
    return abstract_state_ids;
}

static int compute_max_h_with_statistics(
    const vector<CostPartitioningHeuristic> &cp_heuristics,
    const vector<int> &abstract_state_ids) {
    int max_h = 0;
    for (const CostPartitioningHeuristic &cp_heuristic : cp_heuristics) {
        int sum_h = cp_heuristic.compute_heuristic(abstract_state_ids);
        if (sum_h == INF) {
            // Silvan: exit the loop once INF is the best value
            max_h = sum_h;
            break;
        }
        if (sum_h > max_h) {
            max_h = sum_h;
        }
    }
    assert(max_h >= 0);

    return max_h;
}

int SaturatedCostPartitionings::compute_value(const State &state) {
    vector<int> abstract_state_ids = get_abstract_state_ids(
        abstraction_functions, state);
    int h = compute_max_h_with_statistics(cp_heuristics, abstract_state_ids);
    if (h == INF) {
        return INF;
    }
    return h;
}

int SaturatedCostPartitionings::get_number_of_factors() const {
    return num_original_abstractions;
}

SaturatedCostPartitioningsFactory::SaturatedCostPartitioningsFactory(
    const Options &opts)
    : CostPartitioningFactory(),
      order_generator(opts.get<shared_ptr<OrderGenerator>>("order_generator")),
      max_orders(opts.get<int>("max_orders")),
      max_time(opts.get<double>("max_time")),
      diversify(opts.get<bool>("diversify")),
      num_samples(opts.get<int>("samples")),
      max_optimization_time(opts.get<double>("max_optimization_time")),
      rng(utils::parse_rng_from_options(opts)),
      sampling_with_dead_ends(SamplingWithDeadEnds(opts.get_enum("sampling_with_dead_ends"))) {
}

void SaturatedCostPartitioningsFactory::initialize(const std::shared_ptr<AbstractTask> &task_) {
    task = task_;
}

CostPartitioningHeuristic compute_scp(
    const Abstractions &abstractions,
    const std::vector<int> &order,
    const std::vector<int> &label_costs) {

    assert(abstractions.size() == order.size());
    int num_labels = label_costs.size();
    CostPartitioningHeuristic cp_heuristic;
    vector<int> remaining_costs = label_costs;
    utils::Verbosity verbosity = utils::Verbosity::SILENT;
    for (size_t i = 0; i < order.size(); ++i) {
        int pos = order[i];
        const Abstraction &abstraction = *abstractions[pos];
        vector<int> h_values = compute_goal_distances_for_abstraction(
            abstraction, remaining_costs, verbosity);
        vector<int> saturated_costs = compute_saturated_costs_for_abstraction(
            abstraction, h_values, num_labels, verbosity);
        cp_heuristic.add_h_values(
            pos, move(h_values), abstraction.merge_and_shrink_representation->is_total());
        if (i == order.size() - 1) {
            break;
        }
        reduce_costs(remaining_costs, saturated_costs);
    }
    return cp_heuristic;
}

static vector<int> get_abstract_state_ids(
    const Abstractions &abstractions, const State &state) {
    vector<int> abstract_state_ids;
    abstract_state_ids.reserve(abstractions.size());
    for (auto &abstraction : abstractions) {
        if (abstraction) {
            // Only add local state IDs for useful abstractions.
            abstract_state_ids.push_back(abstraction->merge_and_shrink_representation->get_value(state));
        } else {
            // Add dummy value if abstraction will never be used.
            abstract_state_ids.push_back(-1);
        }
    }
    return abstract_state_ids;
}

static vector<vector<int>> sample_states_and_return_abstract_state_ids(
    const TaskProxy &task_proxy,
    const Abstractions &abstractions,
    sampling::RandomWalkSampler &sampler,
    int num_samples,
    int init_h,
    const DeadEndDetector &is_dead_end,
    double max_sampling_time) {
    assert(num_samples >= 1);
    utils::CountdownTimer sampling_timer(max_sampling_time);
    utils::Log() << "Start sampling" << endl;
    vector<vector<int>> abstract_state_ids_by_sample;
    abstract_state_ids_by_sample.push_back(
        get_abstract_state_ids(abstractions, task_proxy.get_initial_state()));
    while (static_cast<int>(abstract_state_ids_by_sample.size()) < num_samples
           && !sampling_timer.is_expired()) {
        abstract_state_ids_by_sample.push_back(
            get_abstract_state_ids(abstractions, sampler.sample_state(init_h, is_dead_end)));
    }
    utils::Log() << "Samples: " << abstract_state_ids_by_sample.size() << endl;
    utils::Log() << "Sampling time: " << sampling_timer.get_elapsed_time() << endl;
    return abstract_state_ids_by_sample;
}

unique_ptr<CostPartitioning> single_cp(
    vector<int> &&costs,
    vector<unique_ptr<Abstraction>> &&abstractions) {
    vector<CostPartitioningHeuristic> cp_heuristics;
    cp_heuristics.reserve(1);
    cp_heuristics.push_back(compute_scp(abstractions, get_default_order(abstractions.size()), costs));
    return utils::make_unique_ptr<SaturatedCostPartitionings>(move(abstractions), move(cp_heuristics));
}

unique_ptr<CostPartitioning> SaturatedCostPartitioningsFactory::generate(
    vector<int> &&costs,
    vector<unique_ptr<Abstraction>> &&abstractions,
    utils::Verbosity verbosity) {
    if (verbosity >= utils::Verbosity::DEBUG) {
        cout << "Generating multiple SCP M&S heuristics for given abstractions..." << endl;
    }

    if (abstractions.size() == 1) {
        return single_cp(move(costs), move(abstractions));
    }

    utils::Log log;
    utils::CountdownTimer timer(max_time);
    log << "Number of abstractions: " << abstractions.size() << endl;

    DeadEndDetector real_is_dead_end =
        [&abstractions](const State &state) {
            vector<int> abstract_state_ids = get_abstract_state_ids(abstractions, state);
            return any_of(abstract_state_ids.begin(), abstract_state_ids.end(), [](int i){return i == PRUNED_STATE;});
        };
    DeadEndDetector no_is_dead_end = [](const State &) { return false;};

    TaskProxy task_proxy(*task);
    State initial_state = task_proxy.get_initial_state();

    // If the unsolvability heuristic detects unsolvability in the initial state,
    // we don't need any orders.
    if (real_is_dead_end(initial_state)) {
        log << "Initial state is unsolvable." << endl;
        return {};
    }

    order_generator->initialize(task_proxy);

    // Compute h(s_0) using a greedy order for s_0.
    vector<int> abstract_state_ids_for_init = get_abstract_state_ids(
        abstractions, initial_state);
    Order order_for_init = order_generator->compute_order(
        abstractions, costs, verbosity, abstract_state_ids_for_init);
    CostPartitioningHeuristic cp_for_init = compute_scp(
        abstractions, order_for_init, costs);
    int init_h = cp_for_init.compute_heuristic(abstract_state_ids_for_init);
    if (init_h == INF) {
        log << "Initial state is unsolvable." << endl;
        return {};
    }

    sampling::RandomWalkSampler sampler(task_proxy, *rng);

    unique_ptr<Diversifier> diversifier;
    if (diversify) {
        double max_sampling_time = timer.get_remaining_time();
        DeadEndDetector is_dead_end = no_is_dead_end;
        if (sampling_with_dead_ends == SamplingWithDeadEnds::Div ||
            sampling_with_dead_ends == SamplingWithDeadEnds::DivAndOpt) {
            is_dead_end = real_is_dead_end;
        }
        diversifier = utils::make_unique_ptr<Diversifier>(
            sample_states_and_return_abstract_state_ids(
                task_proxy, abstractions, sampler, num_samples, init_h, is_dead_end, max_sampling_time));
    }

    DeadEndDetector is_dead_end = no_is_dead_end;
    if (sampling_with_dead_ends == SamplingWithDeadEnds::Opt ||
        sampling_with_dead_ends == SamplingWithDeadEnds::DivAndOpt) {
        is_dead_end = real_is_dead_end;
    }

    log << "Start computing cost partitionings" << endl;
    vector<CostPartitioningHeuristic> cp_heuristics;
    int evaluated_orders = 0;
    while (static_cast<int>(cp_heuristics.size()) < max_orders &&
           (!timer.is_expired() || cp_heuristics.empty())) {
        bool first_order = (evaluated_orders == 0);

        vector<int> abstract_state_ids;
        Order order;
        CostPartitioningHeuristic cp_heuristic;
        if (first_order) {
            // Use initial state as first sample.
            abstract_state_ids = abstract_state_ids_for_init;
            order = order_for_init;
            cp_heuristic = cp_for_init;
        } else {
            abstract_state_ids = get_abstract_state_ids(
                abstractions, sampler.sample_state(init_h, is_dead_end));
            order = order_generator->compute_order(
                abstractions, costs, verbosity, abstract_state_ids);
            cp_heuristic = compute_scp(abstractions, order, costs);
        }

        // Optimize order.
        double optimization_time = min(
            static_cast<double>(timer.get_remaining_time()), max_optimization_time);
        if (optimization_time > 0) {
            utils::CountdownTimer opt_timer(optimization_time);
            int incumbent_h_value = cp_heuristic.compute_heuristic(abstract_state_ids);
            if (incumbent_h_value != INF) {
                // Silvan: we cannot improve upon INF
                optimize_order_with_hill_climbing(
                    opt_timer, abstractions, costs, abstract_state_ids, order,
                    cp_heuristic, incumbent_h_value, first_order);
            }
            if (first_order) {
                log << "Time for optimizing order: " << opt_timer.get_elapsed_time()
                    << endl;
            }
        }

        // If diversify=true, only add order if it improves upon previously
        // added orders.
        if (!diversifier || diversifier->is_diverse(cp_heuristic)) {
            cp_heuristics.push_back(move(cp_heuristic));
            if (diversifier) {
                log << "Sum over max h values for " << num_samples
                    << " samples after " << timer.get_elapsed_time()
                    << " of diversification: "
                    << diversifier->compute_sum_portfolio_h_value_for_samples()
                    << endl;
            }
        }

        ++evaluated_orders;
    }

    log << "Evaluated orders: " << evaluated_orders << endl;
    log << "Cost partitionings: " << cp_heuristics.size() << endl;
    log << "Time for computing cost partitionings: " << timer.get_elapsed_time()
        << endl;

    return utils::make_unique_ptr<SaturatedCostPartitionings>(move(abstractions), move(cp_heuristics));
}

static shared_ptr<SaturatedCostPartitioningsFactory>_parse(OptionParser &parser) {
    parser.add_option<shared_ptr<OrderGenerator>>(
        "order_generator",
        "order generator",
        "greedy_orders()");
    parser.add_option<int>(
        "max_orders",
        "maximum number of orders",
        "infinity",
        Bounds("0", "infinity"));
    parser.add_option<double>(
        "max_time",
        "maximum time for finding orders",
        "200.0",
        Bounds("0", "infinity"));
    parser.add_option<bool>(
        "diversify",
        "only keep orders that have a higher heuristic value than all previous"
        " orders for any of the samples",
        "true");
    parser.add_option<int>(
        "samples",
        "number of samples for diversification",
        "1000",
        Bounds("1", "infinity"));
    parser.add_option<double>(
        "max_optimization_time",
        "maximum time for optimizing each order with hill climbing",
        "2.0",
        Bounds("0.0", "infinity"));
    parser.add_option<bool>(
        "store_unsolvable_states_once",
        "store unsolvable states once per abstraction, instead of once per order. "
        "If store_unsolvable_states_once=true, we store unsolvable states in "
        "UnsolvabilityHeuristic. If store_unsolvable_states_once=false, we "
        "additionally store them in the lookup tables. In any case, we use "
        "UnsolvabilityHeuristic to detect unsolvable states. "
        "(this option only affects the saturated_cost_partitioning() plugin)",
        "true");
    utils::add_rng_options(parser);

    vector<string> sampling_names;
    vector<string> sampling_doc;
    sampling_names.push_back("none");
    sampling_doc.push_back("no dead-end detector is used");
    sampling_names.push_back("div");
    sampling_doc.push_back("only use dead-end detector for diversifier");
    sampling_names.push_back("opt");
    sampling_doc.push_back("only use dead-end detector for optimizer");
    sampling_names.push_back("divandopt");
    sampling_doc.push_back("use dead-end detector for both diversifier and optimizer");
    parser.add_enum_option(
        "sampling_with_dead_ends",
        sampling_names,
        "Decide if and when to use a dead-end detector for sampling.",
        "divandopt",
        sampling_doc);

    Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    if (parser.dry_run())
        return nullptr;
    else
        return make_shared<SaturatedCostPartitioningsFactory>(opts);
}

static Plugin<CostPartitioningFactory> _plugin("scps", _parse);
}
