#include "order_optimizer.h"

#include "saturated_cost_partitionings.h"

#include "../utils/countdown_timer.h"
#include "../utils/logging.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
static void log_better_order(const vector<int> &order, int h, int i, int j) {
    utils::g_log << "Switch positions " << i << " and " << j << " (abstractions "
                 << order[j] << ", " << order[i] << "): h=" << h << endl;
    utils::g_log << "Found improving order with h=" << h << ": " << order << endl;
}

static bool search_improving_successor(
    const utils::CountdownTimer &timer,
    const Abstractions &abstractions,
    const vector<int> &costs,
    const vector<int> &abstract_state_ids,
    vector<int> &incumbent_order,
    CostPartitioningHeuristic &incumbent_cp,
    int &incumbent_h_value,
    bool verbose) {
    int num_abstractions = abstractions.size();
    for (int i = 0; i < num_abstractions && !timer.is_expired(); ++i) {
        for (int j = i + 1; j < num_abstractions && !timer.is_expired(); ++j) {
            swap(incumbent_order[i], incumbent_order[j]);

            CostPartitioningHeuristic neighbor_cp =
                compute_scp(abstractions, incumbent_order, costs);

            int h = neighbor_cp.compute_heuristic(abstract_state_ids);
            // Silvan: no need for special cases with INF here
            if (h > incumbent_h_value) {
                incumbent_cp = move(neighbor_cp);
                incumbent_h_value = h;
                if (verbose) {
                    log_better_order(incumbent_order, h, i, j);
                }
                return true;
            } else {
                // Restore incumbent order.
                swap(incumbent_order[i], incumbent_order[j]);
            }
        }
    }
    return false;
}


void optimize_order_with_hill_climbing(
    const utils::CountdownTimer &timer,
    const Abstractions &abstractions,
    const vector<int> &costs,
    const vector<int> &abstract_state_ids,
    vector<int> &incumbent_order,
    CostPartitioningHeuristic &incumbent_cp,
    int incumbent_h_value,
    bool verbose) {
    if (verbose) {
        utils::g_log << "Incumbent h value: " << incumbent_h_value << endl;
    }
    while (!timer.is_expired()) {
        bool success = search_improving_successor(
            timer, abstractions, costs, abstract_state_ids,
            incumbent_order, incumbent_cp, incumbent_h_value, verbose);
        if (!success) {
            break;
        }
    }
}
}