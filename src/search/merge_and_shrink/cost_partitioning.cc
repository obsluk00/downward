#include "cost_partitioning.h"

#include "merge_and_shrink_representation.h"

#include "../plugin.h"

using namespace std;

namespace merge_and_shrink {
Abstraction::Abstraction(
    const TransitionSystem *transition_system,
    unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation,
    const vector<int> &label_mapping)
    : transition_system(transition_system),
      merge_and_shrink_representation(move(merge_and_shrink_representation)),
      label_mapping(label_mapping) {
}

Abstraction::~Abstraction() {
}

unique_ptr<MergeAndShrinkRepresentation> Abstraction::extract_abstraction_function() {
    return move(merge_and_shrink_representation);
}

static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
