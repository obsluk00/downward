#include "max_scp_ms_heuristic.h"

#include "distances.h"
#include "factored_transition_system.h"
#include "label_equivalence_relation.h"
#include "labels.h"
#include "merge_and_shrink_algorithm.h"
#include "merge_and_shrink_representation.h"
#include "transition_system.h"
#include "types.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../task_utils/task_properties.h"

#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/system.h"

#include <cassert>
#include <iostream>
#include <utility>

using namespace std;
using utils::ExitCode;

namespace merge_and_shrink {
MaxSCPMSHeuristic::MaxSCPMSHeuristic(const options::Options &opts)
    : Heuristic(opts) {
    cout << "Initializing maximum SCP merge-and-shrink heuristic..." << endl;
    MergeAndShrinkAlgorithm algorithm(opts);
    scp_ms_heuristics = move(algorithm.compute_scp_ms_heuristics(task_proxy));
    cout << "Done initializing maximum SCP merge-and-shrink heuristic." << endl << endl;
}

int MaxSCPMSHeuristic::compute_heuristic(const GlobalState &global_state) {
    State state = convert_global_state(global_state);
    int max_h = MINUSINF;
    for (const SCPMSHeuristic &scp_ms_heuristic : scp_ms_heuristics.scp_ms_heuristics) {
        int h_val = 0;
        for (size_t factor_index = 0; factor_index < scp_ms_heuristic.mas_representation_raw_ptrs.size(); ++factor_index) {
            int abstract_state = scp_ms_heuristic.mas_representation_raw_ptrs[factor_index]->get_value(state);
            if (abstract_state == PRUNED_STATE)  {
                // If the state has been pruned, we encountered a dead end.
                return DEAD_END;
            }
            int cost = scp_ms_heuristic.goal_distances[factor_index][abstract_state];
            if (cost == INF) {
                // If the state is unreachable or irrelevant, we encountered a dead end.
                return DEAD_END;
            }
            h_val += cost;
        }
        max_h = max(max_h, h_val);
    }
    return max_h;
}

static shared_ptr<Heuristic> _parse(options::OptionParser &parser) {
    parser.document_synopsis(
        "Maximum SCP merge-and-shrink heuristic",
        "The maximum heuristic computed over SCP heuristics computed over "
        "M&S abstractions.");

    Heuristic::add_options_to_parser(parser);
    add_merge_and_shrink_algorithm_options_to_parser(parser);
    options::Options opts = parser.parse();
    if (parser.help_mode()) {
        return nullptr;
    }

    handle_shrink_limit_options_defaults(opts);

    if (parser.dry_run()) {
        return nullptr;
    } else {
        return make_shared<MaxSCPMSHeuristic>(opts);
    }
}

static options::Plugin<Evaluator> _plugin("max_scp_ms", _parse);
}
