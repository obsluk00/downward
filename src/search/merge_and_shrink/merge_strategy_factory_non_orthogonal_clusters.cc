#include "merge_strategy_factory_non_orthogonal_clusters.h"

#include "merge_strategy_non_orthogonal_clusters.h"
#include "merge_selector.h"
#include "merge_tree_factory.h"
#include "transition_system.h"

#include "../task_proxy.h"

#include "../plugins/plugin.h"
#include "../task_utils/causal_graph.h"
#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/system.h"
#include "../utils/rng_options.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>

using namespace std;

namespace utils {
    class RandomNumberGenerator;
}

namespace merge_and_shrink {

    MergeStrategyFactoryNonOrthogonalClusters::MergeStrategyFactoryNonOrthogonalClusters(const plugins::Options &options)
            : MergeStrategyFactory(options),
              rng(utils::parse_rng_from_options(options)),
              combine_strategy(options.get<CombineStrategy>("combine_strategy")),
              cluster_strategy(options.get<ClusterStrategy>("cluster_strategy")),
              tokens(options.get<int>("tokens")),
              merge_selector(options.get<shared_ptr<MergeSelector>>("merge_selector")){
    }

    unique_ptr<MergeStrategy> MergeStrategyFactoryNonOrthogonalClusters::compute_merge_strategy(
            const TaskProxy &task_proxy, const FactoredTransitionSystem &fts) {
        VariablesProxy vars = task_proxy.get_variables();
        causal_graph::CausalGraph cg = task_proxy.get_causal_graph();
        vector<vector<int>> clusters;

        switch(cluster_strategy) {
            case ClusterStrategy::PREDECESSORS:
                for (VariableProxy var : vars) {
                    vector<int> predecessors = cg.get_predecessors(var.get_id());
                    if (predecessors.size() > 0) {
                        // TODO: this would be a lot nicer if we insert at the right spot (same for other cases)
                        predecessors.emplace_back(var.get_id());
                        std::sort(predecessors.begin(), predecessors.end());
                        clusters.push_back(predecessors);
                    }
                }
                break;
            case ClusterStrategy::SUCCESSORS:
                for (VariableProxy var : vars) {
                    vector<int> successors = cg.get_successors(var.get_id());
                    if (successors.size() > 0) {
                        successors.emplace_back(var.get_id());
                        std::sort(successors.begin(), successors.end());
                        clusters.push_back(successors);
                    }
                }
                break;
            case ClusterStrategy::BOTH:
                for (VariableProxy var : vars) {
                    vector<int> predecessors = cg.get_predecessors(var.get_id());
                    vector<int> successors = cg.get_successors(var.get_id());
                    vector<int> both;
                    set_union(
                            predecessors.begin(), predecessors.end(),
                            successors.begin(), successors.end(),
                            back_inserter(both));
                    if (both.size() > 0) {
                        both.emplace_back(var.get_id());
                        std::sort(both.begin(), both.end());
                        clusters.push_back(both);
                    }
                }
                break;
        }

        map<int, int> var_count = compute_var_count(clusters, task_proxy);
        int times_to_clone = compute_times_to_clone(var_count, vars.size());

        if (combine_strategy != CombineStrategy::TOTAL) {
            // TODO: might find a more elegant way for this, needs to be ordered so that equality checks in combination steps work
            //  -> optimize insertion of variable into its own cluster
            for (vector<int> cluster : clusters) {
                sort(cluster.begin(), cluster.end());
            }
            while (tokens < times_to_clone) {
                clusters = combine_clusters(clusters, combine_strategy);
                var_count = compute_var_count(clusters, task_proxy);
                times_to_clone = compute_times_to_clone(var_count, vars.size());
            }
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

    int MergeStrategyFactoryNonOrthogonalClusters::compute_times_to_clone(std::map<int, int> var_count, int variable_count) {
        int times_to_clone = 0;
        map<int, int>::iterator it = var_count.begin();
        while (it != var_count.end()) {
            times_to_clone += it->second;
            it++;
        }
        return times_to_clone - variable_count;
    }

    map<int,int> MergeStrategyFactoryNonOrthogonalClusters::compute_var_count(vector<vector<int>> clusters, const TaskProxy &task_proxy) {
        VariablesProxy vars = task_proxy.get_variables();
        map<int, int> var_count;
        for (VariableProxy var : vars) {
            var_count[var.get_id()] = 0;
        }
        for (vector<int> cluster : clusters) {
            for (int var_id : cluster) {
                var_count[var_id] += 1;
            }
        }
        return var_count;
    }

    vector<vector<int>> MergeStrategyFactoryNonOrthogonalClusters::combine_clusters(std::vector<std::vector<int>> clusters,
                                                                     merge_and_shrink::CombineStrategy combine_strategy) {
        assert(clusters.size() > 2 && combine_strategy != CombineStrategy::TOTAL);
        // TODO: shuffle before checking, this also randomly tiebreaks if there are more than 2 smallest/largest
        int index_1 = 0;
        int index_2 = 1;
        switch (combine_strategy) {
            case CombineStrategy::COMBINE_SMALLEST:
                for (int i = 0; i < clusters.size(); i++) {
                    if (clusters[i].size() < clusters[index_1].size()) {
                        index_2 = index_1;
                        index_1 = i;
                    } else if (clusters[i].size() < clusters[index_2].size() && i != index_1)
                        index_2 = i;
                }
                break;
            case CombineStrategy::COMBINE_LARGEST:
                for (int i = 0; i < clusters.size(); i++) {
                    if (clusters[i].size() > clusters[index_1].size()) {
                        index_2 = index_1;
                        index_1 = i;
                    } else if (clusters[i].size() > clusters[index_2].size() && i != index_1)
                        index_2 = i;
                }
                break;
            case CombineStrategy::RANDOM:
                // TODO: figure out whats wrong here
                //first_to_combine = rng->
                //second_to_combine = rng->choose(clusters);
                //while (first_to_combine == second_to_combine)
                //    second_to_combine = rng->choose(clusters);
                break;
            case CombineStrategy::LARGEST_OVERLAP:
                // TODO: implement if its worth it
                break;
        }
        vector<int> combined;
        set_union(
                clusters[index_1].begin(), clusters[index_1].end(),
                clusters[index_2].begin(), clusters[index_2].end(),
                back_inserter(combined));
        if (index_1 > index_2) {
            int temp = index_1;
            index_1 = index_2;
            index_2 = temp;
        }
        clusters.erase(clusters.begin() + index_1);
        clusters.erase(clusters.begin() + index_2 - 1);
        for (vector<int> cluster : clusters) {
            if (cluster == combined)
                return clusters;
        }
        clusters.emplace_back(combined);
        return clusters;
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
            switch (combine_strategy) {
                case CombineStrategy::TOTAL:
                    log << "Ignore the limit";
                    break;
                case CombineStrategy::COMBINE_LARGEST:
                    log << "Combine the largest clusters";
                    break;
                case CombineStrategy::COMBINE_SMALLEST:
                    log << "Combine smallest clusters";
                    break;
                case CombineStrategy::RANDOM:
                    log << "Combine random clusters";
                    break;
                case CombineStrategy::LARGEST_OVERLAP:
                    log << "Combine clusters with largest overlap";
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
            add_option<CombineStrategy>(
                    "combine_strategy",
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

            utils::add_rng_options(*this);
        }

        virtual shared_ptr<MergeStrategyFactoryNonOrthogonalClusters> create_component(const plugins::Options &options, const utils::Context &context) const override {
            return make_shared<MergeStrategyFactoryNonOrthogonalClusters>(options);
        }
    };

    static plugins::FeaturePlugin<MergeStrategyFactoryNonOrthogonalClustersFeature> _plugin;

    static plugins::TypedEnumPlugin<CombineStrategy> _enum_plugin1({
                                                                     {"total",
                                                                             ""},
                                                                     {"combine_smallest",
                                                                             ""},
                                                                     {"combine_largest",
                                                                             ""},
                                                                     {"random",
                                                                             ""},
                                                                     {"larges_overlap",
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