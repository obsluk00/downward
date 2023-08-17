#include "cluster_factory_causal_graph_neighbors.h"

#include "factored_transition_system.h"

#include "../task_proxy.h"
#include "../plugins/plugin.h"
#include "../task_utils/causal_graph.h"

#include <cassert>

using namespace std;

namespace merge_and_shrink {
ClusterFactoryCausalGraphNeighbors::ClusterFactoryCausalGraphNeighbors(
    const plugins::Options &options)
    : cluster_strategy(options.get_list<ArcChoice>("cluster_strategy"))
      {
}

vector<vector<int>> ClusterFactoryCausalGraphNeighbors::create_clusters(const TaskProxy &task_proxy) const {
    vector<vector<int>> clusters;
    VariablesProxy vars = task_proxy.get_variables();
    causal_graph::CausalGraph cg = task_proxy.get_causal_graph();

    for (VariableProxy var : vars) {
        clusters.emplace_back(vector<int>());
    }

    for (ArcChoice arcs : cluster_strategy) {
        switch(arcs) {
            case ArcChoice::PRE_EFF:
                for (VariableProxy var : vars) {
                    const vector<int> &predecessors = cg.get_predecessors(var.get_id());
                    if (predecessors.size() > 0) {
                        clusters.push_back(predecessors);
                        clusters.back().push_back(var.get_id());
                    }
                }
        }
    }
    switch(cluster_strategy) {
        case ClusterStrategy::PREDECESSORS:
            for (VariableProxy var : vars) {
                const vector<int> &predecessors = cg.get_predecessors(var.get_id());
                for (int var_id : predecessors) {
                    var_count[var_id] += 1;
                }
                if (predecessors.size() > 0) {
                    clusters.push_back(predecessors);
                    clusters.back().push_back(var.get_id());
                }
            }
            break;
        case ClusterStrategy::SUCCESSORS:
            for (VariableProxy var : vars) {
                const vector<int> &successors = cg.get_successors(var.get_id());
                for (int var_id : successors) {
                    var_count[var_id] += 1;
                }
                if (successors.size() > 0) {
                    clusters.push_back(successors);
                    clusters.back().push_back(var.get_id());
                }
            }
            break;
        case ClusterStrategy::BOTH:
            for (VariableProxy var : vars) {
                const vector<int> &predecessors = cg.get_predecessors(var.get_id());
                const vector<int> &successors = cg.get_successors(var.get_id());
                vector<int> both;
                set_union(
                        predecessors.begin(), predecessors.end(),
                        successors.begin(), successors.end(),
                        back_inserter(both));
                for (int var_id : both) {
                    var_count[var_id] += 1;
                }
                if (both.size() > 0) {
                    both.push_back(var.get_id());
                    clusters.push_back(both);
                }
            }
            break;
    }

    return clusters;
}

string ClusterFactoryCausalGraphNeighbors::name() const {
    return "causal graph neighbour clustering";
}

void ClusterFactoryCausalGraphNeighbors::dump_specific_options(
    utils::LogProxy &log) const {
    if (log.is_at_least_normal()) {
        log << "Clusters built from " << endl;
        for (const ArcChoice i : cluster_strategy) {
            switch (i) {
                case ArcChoice::PRE_EFF:
                    log << "pre-eff arcs" << endl;
                    break;
                case ArcChoice::EFF_EFF:
                    log << "eff-eff arcs" << endl;
                    break;
                case ArcChoice::EFF_PRE:
                    log << "eff-pre arcs" << endl;
                    break;
            }
        }
        log << "Neighbourhoods are of depth " << depth << "." << endl;
    }
}


class ClusterFactoryCausalGraphNeighborsFeature : public plugins::TypedFeature<ClusterFactory, ClusterFactoryCausalGraphNeighbors> {
public:
   ClusterFactoryCausalGraphNeighborsFeature() : TypedFeature("causal_graph_neighbors") {
        document_title("Causal graph neighborhood based clustering");
        document_synopsis(
            "This clustering strategy creates clusters by taking neighbors of variables according to specified arcs.");

        add_list_option<ArcChoice>(
            "arc_choices",
            "The list of scoring functions used to compute scores for candidates.");

       add_option<int>(
               "depth",
               "depth of the clusters");
    }
};

static plugins::FeaturePlugin<ClusterFactoryCausalGraphNeighborsFeature> _plugin;
}
