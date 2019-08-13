#include "max_cp_ms_heuristic.h"

#include "cost_partitioning.h"
#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"

#include <iostream>
#include <utility>

using namespace std;
using utils::ExitCode;

namespace merge_and_shrink {
MaxCPMSHeuristic::MaxCPMSHeuristic(const options::Options &opts)
    : Heuristic(opts) {
    cout << "Initializing maximum CP merge-and-shrink heuristic..." << endl;
    shared_ptr<CostPartitioningFactory> cp_factory = opts.get<shared_ptr<CostPartitioningFactory>>("cost_partitioning");
    cost_partitionings = cp_factory->generate(task_proxy);
    int num_cps= cost_partitionings.size();
    if (!num_cps) {
        cerr << "Got 0 cost partitionings" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
    cout << "Done initializing maximum CP merge-and-shrink heuristic." << endl << endl;
}

int MaxCPMSHeuristic::compute_heuristic(const GlobalState &global_state) {
    State state = convert_global_state(global_state);
    int max_h = MINUSINF;
    for (const auto &cp: cost_partitionings) {
        int h_val = cp->compute_value(state);
        if (h_val == INF) {
            return DEAD_END;
        }
        max_h = max(max_h, h_val);
    }
    return max_h;
}

static shared_ptr<Heuristic> _parse(options::OptionParser &parser) {
    parser.document_synopsis(
        "Maximum CP merge-and-shrink heuristic",
        "The maximum heuristic computed over CP heuristics computed over "
        "M&S abstractions.");
    parser.add_option<shared_ptr<CostPartitioningFactory>>(
        "cost_partitioning",
        "method to generate cost partitionings");

    Heuristic::add_options_to_parser(parser);

    Options opts = parser.parse();
    if (parser.dry_run()) {
        return nullptr;
    } else {
        return make_shared<MaxCPMSHeuristic>(opts);
    }
}

static options::Plugin<Evaluator> _plugin("max_cp_ms", _parse);
}
