#include "cost_partitioning.h"

#include "merge_and_shrink_representation.h"

#include "../plugins/plugin.h"

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

static class CostPartitioningFactoryCategoryPlugin : public plugins::TypedCategoryPlugin<CostPartitioningFactory> {
public:
    CostPartitioningFactoryCategoryPlugin() : TypedCategoryPlugin("CostPartitioning") {
        document_synopsis(
            "This page describes the various cost partitioning generation "
            "methods.");
    }
}
_category_plugin;
}
