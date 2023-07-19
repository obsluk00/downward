#include "max_cp_ms_heuristic.h"

#include "cp_mas.h"
#include "cp_mas_non_orthogonal.h"
#include "cost_partitioning.h"
#include "types.h"

#include "../plugins/options.h"
#include "../plugins/plugin.h"

#include <iostream>
#include <utility>

using namespace std;
using utils::ExitCode;

namespace merge_and_shrink {
MaxCPMSHeuristic::MaxCPMSHeuristic(const plugins::Options &opts)
    : Heuristic(opts) {
    if (opts.get<bool>("non_orthogonal")) {
        CPMAS algorithm(opts);
        cost_partitionings = algorithm.compute_cps(task);
        if (cost_partitionings.empty()) {
            cerr << "Got 0 cost partitionings" << endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
    } else {
        CPMAS algorithm(opts);
        cost_partitionings = algorithm.compute_cps(task);
        if (cost_partitionings.empty()) {
            cerr << "Got 0 cost partitionings" << endl;
            utils::exit_with(utils::ExitCode::SEARCH_CRITICAL_ERROR);
        }
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

class MaxCPMSHeuristicFeature : public plugins::TypedFeature<Evaluator, MaxCPMSHeuristic> {
public:
    MaxCPMSHeuristicFeature() : TypedFeature("max_cp_ms") {
        document_synopsis(
            "Maximum CP merge-and-shrink heuristic. The maximum heuristic "
            "computed over CP heuristics computed over M&S abstractions.");

        Heuristic::add_options_to_feature(*this);
        add_cp_merge_and_shrink_algorithm_options_to_feature(*this);
    }

    virtual shared_ptr<MaxCPMSHeuristic> create_component(const plugins::Options &options, const utils::Context &context) const override {
        plugins::Options options_copy(options);
        handle_cp_merge_and_shrink_algorithm_options(options_copy, context);
        return make_shared<MaxCPMSHeuristic>(options_copy);
    }
};

static plugins::FeaturePlugin<MaxCPMSHeuristicFeature> _plugin;
}
