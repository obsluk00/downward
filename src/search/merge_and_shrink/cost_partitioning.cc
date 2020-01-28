#include "cost_partitioning.h"

#include "cp_mas_interleaved.h"
#include "cp_mas_offline.h"
#include "merge_and_shrink_representation.h"

#include "../plugin.h"

using namespace std;

namespace merge_and_shrink {
Abstraction::Abstraction(
    const TransitionSystem *transition_system,
    std::unique_ptr<MergeAndShrinkRepresentation> merge_and_shrink_representation,
    int fts_index)
    : transition_system(transition_system),
      merge_and_shrink_representation(move(merge_and_shrink_representation)),
      fts_index(fts_index) {
}

Abstraction::~Abstraction() {
}

CostPartitioningFactory::CostPartitioningFactory(const options::Options &opts) :
    single_cp(opts.get<bool>("single_cp")) {
}

vector<unique_ptr<CostPartitioning>> CostPartitioningFactory::generate(
    const options::Options &opts, const TaskProxy &task_proxy) {
    initialize(task_proxy);
    if (single_cp) {
        CPMASOffline algorithm(opts);
        vector<unique_ptr<CostPartitioning>> result;
        result.reserve(1);
        result.push_back(algorithm.compute_single_ms_cp(task_proxy, *this));
        return result;
    } else {
        CPMASInterleaved algorithm(opts);
        return algorithm.compute_ms_cps(task_proxy, *this);
    }
}

void add_cp_options_to_parser(options::OptionParser &parser) {
    parser.add_option<bool>(
        "single_cp",
        "If true, compute a single CP over all abstractions collected through "
        "different snapshots. If false, compute a CP for each snapshot.",
        "false");
}

static options::PluginTypePlugin<CostPartitioningFactory> _type_plugin(
    "CostPartitioning",
    "This page describes the various cost partitioning generation methods.");
}
