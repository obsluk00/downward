#include "cost_partitioning.h"

#include "../options/plugin.h"

using namespace std;

namespace merge_and_shrink {
static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
