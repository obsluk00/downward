#include "cluster_factory.h"

#include "factored_transition_system.h"

#include "../plugins/plugin.h"
#include "../utils/logging.h"

#include <cassert>
#include <iostream>

using namespace std;

namespace merge_and_shrink {

void ClusterFactory::dump_options(utils::LogProxy &log) const {
    if (log.is_at_least_normal()) {
        log << "Merge selector options:" << endl;
        log << "Name: " << name() << endl;
        dump_specific_options(log);
    }
}

// TODO: documentation
static class ClusterFactoryCategoryPlugin : public plugins::TypedCategoryPlugin<ClusterFactory> {
public:
    ClusterFactoryCategoryPlugin() : TypedCategoryPlugin("ClusterFactory") {
        document_synopsis(
            "");
    }
}
_category_plugin;
}
