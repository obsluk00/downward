#include "cost_partitioning.h"

#include "cp_mas_interleaved.h"
#include "cp_mas_offline.h"
#include "merge_and_shrink_representation.h"

#include "../plugin.h"

using namespace std;

namespace merge_and_shrink {
Abstraction::Abstraction(
    const TransitionSystem *transition_system,
    unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation,
    int fts_index,
    const vector<int> &label_mapping)
    : transition_system(transition_system),
      merge_and_shrink_representation(move(merge_and_shrink_representation)),
      fts_index(fts_index),
      label_mapping(label_mapping) {
}

Abstraction::~Abstraction() {
}

static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
