#include "cost_partitioning.h"

#include "merge_and_shrink_representation.h"

#include "../plugin.h"

using namespace std;

namespace merge_and_shrink {
Abstraction::Abstraction(
    const TransitionSystem *transition_system,
    std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation)
    : transition_system(transition_system),
      merge_and_shrink_representation(move(merge_and_shrink_representation)) {
}

Abstraction::~Abstraction() {
}

static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
