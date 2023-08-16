#include "merge_strategy_factory_non_orthogonal_clusters.h"

#include "merge_strategy_non_orthogonal_clusters.h"
#include "merge_selector.h"
#include "merge_tree_factory.h"
#include "transition_system.h"

#include "../task_proxy.h"

#include "../algorithms/sccs.h"
#include "../plugins/plugin.h"
#include "../task_utils/causal_graph.h"
#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/system.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>

using namespace std;

namespace merge_and_shrink {

    MergeStrategyFactoryNonOrthogonalClusters::MergeStrategyFactoryNonOrthogonalClusters(const plugins::Options &options)
            : MergeStrategyFactory(options),
              clone_strategy(options.get<CloneStrategy>("clone_strategy")),
              cluster_strategy(options.get<ClusterStrategy>("cluster_strategy")),
              tokens(options.get<int>("tokens")),
              merge_selector(options.get<shared_ptr<MergeSelector>>("merge_selector")){
    }

    unique_ptr<MergeStrategy> MergeStrategyFactoryNonOrthogonalClusters::compute_merge_strategy(
            const TaskProxy &task_proxy, const FactoredTransitionSystem &fts) {
        VariablesProxy vars = task_proxy.get_variables();
        causal_graph::CausalGraph cg = task_proxy.get_causal_graph();
        map<int, int> var_count;
        for (VariableProxy var : vars) {
            var_count[var.get_id()] = 0;
        }
        vector<vector<int>> clusters;

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

        // TODO: clone count case switch. if not enough tokens for all clusters, merge clusters such that the total of required clones decreases
        switch (clone_strategy) {
            case CloneStrategy::TOTAL:
                break;
            case CloneStrategy::COMBINE_SMALLEST:
                break;
            case CloneStrategy::COMBINE_LARGEST:
                break;

        }

         merge_selector->initialize(task_proxy);


        return utils::make_unique_ptr<MergeStrategyNonOrthogonalClusters>(
                fts,
                task_proxy,
                merge_selector,
                move(clusters),
                move(var_count),
                tokens);
    }

    bool MergeStrategyFactoryNonOrthogonalClusters::requires_init_distances() const {
        return merge_selector->requires_init_distances();
    }

    bool MergeStrategyFactoryNonOrthogonalClusters::requires_goal_distances() const {
        return merge_selector->requires_goal_distances();
    }

    // TODO: output
    void MergeStrategyFactoryNonOrthogonalClusters::dump_strategy_specific_options() const {
        if (log.is_at_least_normal()) {
            log << "Method used to determine how to handle more required clones than available tokens: ";
            switch (clone_strategy) {
                case CloneStrategy::TOTAL:
                    log << "Ignore the limit";
                    break;
                case CloneStrategy::COMBINE_LARGEST:
                    log << "Combine the largest clusters";
                    break;
                case CloneStrategy::COMBINE_SMALLEST:
                    log << "Combine smallest clusters";
                    break;
            }
            log << endl;


            log << "Clusters are being computed by: ";
            switch (cluster_strategy) {
                case ClusterStrategy::PREDECESSORS:
                    log << "using predecessors in the causal graph.";
                    break;
                case ClusterStrategy::SUCCESSORS:
                    log << "using successors in the causal graph.";
                    break;
                case ClusterStrategy::BOTH:
                    log << "using predecessors and successors in the causal graph.";
                    break;
            }
            log << endl;

            log << "Merge strategy for merging within clusters: " << endl;
            merge_selector->dump_options(log);
        }
    }

    string MergeStrategyFactoryNonOrthogonalClusters::name() const {
        return "non_orthogonal_clusters";
    }

    //TODO: description
    class MergeStrategyFactoryNonOrthogonalClustersFeature : public plugins::TypedFeature<MergeStrategyFactory, MergeStrategyFactoryNonOrthogonalClusters> {
    public:
        MergeStrategyFactoryNonOrthogonalClustersFeature() : TypedFeature("merge_non_orthogonal_clusters") {
            document_title("Non-orthogonal cluster based merge strategy");
            document_synopsis(
                    "lorem ipsum dolor sit amet");
            add_option<CloneStrategy>(
                    "clone_strategy",
                    "how to clone if not enough tokens",
                    "complete");
            add_option<ClusterStrategy>(
                    "cluster_strategy",
                    "how to create clusters",
                    "predecessors");
            add_option<shared_ptr<MergeSelector>>(
                    "merge_selector",
                    "the fallback merge strategy to use if a stateless strategy should "
                    "be used.",
                    plugins::ArgumentInfo::NO_DEFAULT);
            add_option<int>(
                    "tokens",
                    "Amount of times the algorithm is allowed to cloned.");
            add_merge_strategy_options_to_feature(*this);
        }

        virtual shared_ptr<MergeStrategyFactoryNonOrthogonalClusters> create_component(const plugins::Options &options, const utils::Context &context) const override {
            return make_shared<MergeStrategyFactoryNonOrthogonalClusters>(options);
        }
    };

    static plugins::FeaturePlugin<MergeStrategyFactoryNonOrthogonalClustersFeature> _plugin;

    static plugins::TypedEnumPlugin<CloneStrategy> _enum_plugin1({
                                                                     {"complete",
                                                                             ""},
                                                                     {"combine_smallest",
                                                                             ""},
                                                                     {"combine_largest",
                                                                             ""},
                                                             });
    static plugins::TypedEnumPlugin<ClusterStrategy> _enum_plugin({
                                                                     {"predecessors",
                                                                             ""},
                                                                     {"successors",
                                                                             ""},
                                                                     {"both",
                                                                                ""}
                                                             });
}
