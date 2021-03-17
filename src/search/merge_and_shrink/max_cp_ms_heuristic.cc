#include "max_cp_ms_heuristic.h"

#include "cp_mas.h"
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
    CPMAS algorithm(opts);
    cost_partitionings = algorithm.compute_cps(task);
    if (cost_partitionings.empty()) {
        cerr << "Got 0 cost partitionings" << endl;
        utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
    }
}

int MaxCPMSHeuristic::compute_heuristic(const State &ancestor_state) {
    State state = convert_ancestor_state(ancestor_state);
    int max_h = -INF;
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

    Heuristic::add_options_to_parser(parser);
    add_cp_merge_and_shrink_algorithm_options_to_parser(parser);
    options::Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    handle_cp_merge_and_shrink_algorithm_options(opts);

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return make_shared<MaxCPMSHeuristic>(opts);
    }
}

static options::Plugin<Evaluator> _plugin("max_cp_ms", _parse);
}
