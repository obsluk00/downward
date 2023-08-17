#ifndef MERGE_AND_SHRINK_CLUSTER_FACTORY_H
#define MERGE_AND_SHRINK_CLUSTER_FACTORY_H

#include <string>
#include <vector>

class TaskProxy;

namespace utils {
class LogProxy;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class ClusterFactory {
protected:
    virtual std::string name() const = 0;
    virtual void dump_specific_options(utils::LogProxy &) const {}
public:
    ClusterFactory() = default;
    virtual ~ClusterFactory() = default;
    virtual std::vector<std::vector<int>> create_clusters(const TaskProxy &task_proxy) const = 0;
    void dump_options(utils::LogProxy &log) const;
};
}

#endif
