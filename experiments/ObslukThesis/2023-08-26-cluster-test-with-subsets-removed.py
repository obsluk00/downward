#! /usr/bin/env python

import os
import shutil

import project
from lab.reports import Attribute
from lab.reports import geometric_mean, arithmetic_mean


REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]
SCP_LOGIN = "obsluk00@login-infai.scicore.unibas.ch"
REMOTE_REPOS_DIR = ""
# If REVISION_CACHE is None, the default ./data/revision-cache is used.
REVISION_CACHE = os.environ.get("DOWNWARD_REVISION_CACHE")
if project.REMOTE:
    SUITE = project.SUITE_OPTIMAL_STRIPS
    ENV = project.BaselSlurmEnvironment(email="luka.obser@unibas.ch", partition="infai_2")
else:
    SUITE = ["depot:p01.pddl", "grid:prob01.pddl", "gripper:prob01.pddl"]
    ENV = project.LocalEnvironment(processes=2)

CONFIGS = [
    (f"depth 0", ["--search", "let(h, merge_and_shrink(verbosity=normal, non_orthogonal=true, shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_non_orthogonal_clusters(merge_selector=score_based_filtering(scoring_functions=[goal_relevance(), dfp(), avoid_existing(), single_random()]), tokens = 100, combine_strategy=random, cluster_strategy=[pre_eff, eff_eff, eff_pre], depth=0) ,label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50k,threshold_before_merge=1), astar(h))"]),
    (f"depth 1", ["--search", "let(h, merge_and_shrink(verbosity=normal, non_orthogonal=true, shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_non_orthogonal_clusters(merge_selector=score_based_filtering(scoring_functions=[goal_relevance(), dfp(), avoid_existing(), single_random()]), tokens = 100, combine_strategy=random, cluster_strategy=[pre_eff, eff_eff, eff_pre], depth=1) ,label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50k,threshold_before_merge=1), astar(h))"]),
    (f"depth 2", ["--search", "let(h, merge_and_shrink(verbosity=normal, non_orthogonal=true, shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_non_orthogonal_clusters(merge_selector=score_based_filtering(scoring_functions=[goal_relevance(), dfp(), avoid_existing(), single_random()]), tokens = 100, combine_strategy=random, cluster_strategy=[pre_eff, eff_eff, eff_pre], depth=2) ,label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50k,threshold_before_merge=1), astar(h))"]),
    (f"depth 5", ["--search", "let(h, merge_and_shrink(verbosity=normal, non_orthogonal=true, shrink_strategy=shrink_bisimulation(greedy=false),merge_strategy=merge_non_orthogonal_clusters(merge_selector=score_based_filtering(scoring_functions=[goal_relevance(), dfp(), avoid_existing(), single_random()]), tokens = 100, combine_strategy=random, cluster_strategy=[pre_eff, eff_eff, eff_pre], depth=5) ,label_reduction=exact(before_shrinking=true,before_merging=false),max_states=50k,threshold_before_merge=1), astar(h))"]),
]
BUILD_OPTIONS = []
DRIVER_OPTIONS = ["--overall-time-limit", "30m"]
REVS = [
    ("39b734b", "clone")
]

non_orthogonality = Attribute("non_orthogonality", absolute=True)
times_cloned = Attribute("times_cloned", absolute=True)
average_cloned = Attribute("average_cloned", absolute=True)
largest_cloned = Attribute("largest_cloned", absolute=True)
ATTRIBUTES = [
"cost",
"coverage",
"error",
"evaluations",
"expansions",
"expansions_until_last_jump",
"generated",
"memory",
"planner_memory",
"planner_time",
"quality",
"run_dir",
"score_evaluations",
"score_expansions",
"score_generated",
"score_memory",
"score_search_time",
"score_total_time",
"search_time",
"total_time",
non_orthogonality,
times_cloned,
average_cloned,
largest_cloned,
]

extra_attributes=[
     Attribute('search_out_of_memory', absolute=True, min_wins=True),
     Attribute('search_out_of_time', absolute=True, min_wins=True),
     Attribute('ms_construction_time', absolute=False, min_wins=True,
function=geometric_mean),
     Attribute('score_ms_construction_time', min_wins=False, digits=4),
     Attribute('ms_atomic_construction_time', absolute=False,
min_wins=True, function=geometric_mean),
     Attribute('ms_abstraction_constructed', absolute=True, min_wins=False),
     Attribute('ms_atomic_fts_constructed', absolute=True, min_wins=False),
     Attribute('ms_out_of_memory', absolute=True, min_wins=True),
     Attribute('ms_out_of_time', absolute=True, min_wins=True),
     Attribute('ms_memory_delta', absolute=False, min_wins=True),
     Attribute('ms_reached_time_limit', absolute=False, min_wins=True),

     Attribute('ms_avg_imperfect_shrinking', absolute=False,
min_wins=True, function=arithmetic_mean),
     Attribute('ms_course_imperfect_shrinking', absolute=True),
     Attribute('ms_course_label_reduction', absolute=True),
     Attribute('ms_init_h_improvements', absolute=False, min_wins=False),
     Attribute('ms_not_exact_iteration', absolute=False, min_wins=False),
     Attribute('ms_one_scc', absolute=True, min_wins=False),
     Attribute('ms_linear_order', absolute=True, min_wins=True),
     Attribute('ms_merge_order', absolute=True),
     Attribute('ms_course_pruning', absolute=True),
     Attribute('ms_avg_pruning', absolute=False, min_wins=False,
function=arithmetic_mean),
     Attribute('ms_tiebreaking_iterations', absolute=True, min_wins=True),
     Attribute('ms_tiebreaking_total', absolute=True, min_wins=True),
     Attribute('ms_max_int_abs_size', absolute=False, min_wins=True,
function=arithmetic_mean),
]


exp = project.FastDownwardExperiment(environment=ENV, revision_cache=REVISION_CACHE)
for config_nick, config in CONFIGS:
    for rev, rev_nick in REVS:
        algo_name = f"{rev_nick}:{config_nick}" if rev_nick else config_nick
        exp.add_algorithm(
            algo_name,
            REPO,
            rev,
            config,
            build_options=BUILD_OPTIONS,
            driver_options=DRIVER_OPTIONS,
        )
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(project.DIR / "parser.py")
exp.add_parser(exp.PLANNER_PARSER)
exp.add_parser(project.DIR / "ms-parser.py")

exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")

if not project.REMOTE:
    exp.add_step("remove-eval-dir", shutil.rmtree, exp.eval_dir, ignore_errors=True)
    project.add_scp_step(exp, SCP_LOGIN, REMOTE_REPOS_DIR)

project.add_absolute_report(
    exp, attributes=ATTRIBUTES + extra_attributes, filter=[project.add_evaluations_per_time]
)

attributes = ["expansions_until_last_jump"]
pairs = [
    ("clone:0", "clone:500"),
]
suffix = "-rel" if project.RELATIVE else ""
for algo1, algo2 in pairs:
    for attr in attributes:
        exp.add_report(
            project.ScatterPlotReport(
                relative=project.RELATIVE,
                get_category=None if project.TEX else lambda run1, run2: run1["domain"],
                attributes=[attr],
                filter_algorithm=[algo1, algo2],
                format="tex" if project.TEX else "png",
            ),
            name=f"{exp.name}-{algo1}-vs-{algo2}-{attr}{suffix}",
        )


exp.run_steps()