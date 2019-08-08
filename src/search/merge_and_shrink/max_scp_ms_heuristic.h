#ifndef MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H
#define MERGE_AND_SHRINK_MAX_SCP_MS_HEURISTIC_H

#include "../heuristic.h"

namespace utils {
enum class Verbosity;
}

namespace merge_and_shrink {
class FactoredTransitionSystem;
class MergeAndShrinkRepresentation;

enum class FactorOrder {
    GIVEN,
    RANDOM
};

struct SCPMSHeuristic {
    std::vector<std::vector<int>> goal_distances;
    std::vector<std::unique_ptr<MergeAndShrinkRepresentation>> mas_representations;
};

struct SCPSnapshotCollector {
    const bool scp_over_atomic_fts;
    const bool scp_over_final_fts;
    const int main_loop_aimed_num_scp_heuristics;
    const int main_loop_iteration_offset_for_computing_scp_heuristics;
    std::function<void (const FactoredTransitionSystem &fts)> add_snapshot;
    utils::Verbosity verbosity;

    SCPSnapshotCollector(
        bool scp_over_atomic_fts,
        bool scp_over_final_fts,
        int main_loop_aimed_num_scp_heuristics,
        int main_loop_iteration_offset_for_computing_scp_heuristics,
        std::function<void (const FactoredTransitionSystem &fts)> add_snapshot,
        utils::Verbosity verbosity);

private:
    double max_time;
    int max_iterations;
    double next_time_to_compute_heuristic;
    int next_iteration_to_compute_heuristic;
    void set_next_time_to_compute_heuristic(int num_computed_scp_heuristics, double current_time);
    void set_next_iteration_to_compute_heuristic(int num_computed_scp_heuristics, int current_iteration);
public:
    void start_main_loop(double max_time, int max_iterations);
    bool compute_next_heuristic(double current_time, int current_iteration, int num_computed_scp_heuristics);
};

class MaxSCPMSHeuristic : public Heuristic {
    std::shared_ptr<utils::RandomNumberGenerator> rng;
    const FactorOrder factor_order;
    const utils::Verbosity verbosity;

    std::vector<SCPMSHeuristic> scp_ms_heuristics;

    SCPMSHeuristic extract_scp_heuristic(
        FactoredTransitionSystem &fts, int index) const;
    SCPMSHeuristic compute_scp_ms_heuristic_over_fts(
        const FactoredTransitionSystem &fts) const;
protected:
    virtual int compute_heuristic(const GlobalState &global_state) override;
public:
    explicit MaxSCPMSHeuristic(const options::Options &opts);
};
}

#endif
