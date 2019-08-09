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

class FTSSnapshotCollector {
private:
    const bool compute_atomic_snapshot;
    const bool compute_final_snapshot;
    const int main_loop_target_num_snapshots;
    const int main_loop_snapshot_each_iteration;
    std::function<void (const FactoredTransitionSystem &fts)> handle_snapshot;
    utils::Verbosity verbosity;

    int num_main_loop_snapshots;
public:
    FTSSnapshotCollector(
        bool compute_atomic_snapshot,
        bool compute_final_snapshot,
        int main_loop_target_num_snapshots,
        int main_loop_snapshot_each_iteration,
        std::function<void (const FactoredTransitionSystem &fts)> handle_snapshot,
        utils::Verbosity verbosity);
    void report_atomic_snapshot(const FactoredTransitionSystem &fts);
    void report_main_loop_snapshot(
        const FactoredTransitionSystem &fts,
        double current_time,
        int current_iteration);
    void report_final_snapshot(const FactoredTransitionSystem &fts);

private:
    double max_time;
    int max_iterations;
    double next_time_to_compute_heuristic;
    int next_iteration_to_compute_heuristic;
    void compute_next_snapshot_time(double current_time);
    void compute_next_snapshot_iteration(int current_iteration);
public:
    void start_main_loop(double max_time, int max_iterations);
    bool compute_next_snapshot(double current_time, int current_iteration);
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
