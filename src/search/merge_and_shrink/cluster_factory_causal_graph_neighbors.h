#ifndef MERGE_AND_SHRINK_CLUSTER_FACTORY_CAUSAL_GRAPH_NEIGHBORS_H
#define MERGE_AND_SHRINK_CLUSTER_FACTORY_CAUSAL_GRAPH_NEIGHBORS_H

#include "cluster_factory.h"

#include "../task_utils/causal_graph.h"

#include <memory>
#include <vector>

namespace plugins {
class Options;
}
enum class ArcChoice {
    PRE_EFF,
    EFF_PRE,
    EFF_EFF
};
namespace merge_and_shrink {
class ClusterFactoryCausalGraphNeighbors : public ClusterFactory {
    std::vector<ArcChoice> cluster_strategy;
    int depth;
protected:
    virtual std::string name() const override;
    virtual void dump_specific_options(utils::LogProxy &log) const override;
public:
    explicit ClusterFactoryCausalGraphNeighbors(const plugins::Options &options);
    virtual ~ClusterFactoryCausalGraphNeighbors() override = default;
    virtual std::vector<std::vector<int>> create_clusters(const TaskProxy &task_proxy) const override;
};
}

#endif
