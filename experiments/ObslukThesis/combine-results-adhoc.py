#! /usr/bin/env python3

from downward.experiment import FastDownwardExperiment
from downward.reports import Attribute
from downward.reports.absolute import AbsoluteReport
from lab.reports import geometric_mean, arithmetic_mean

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

EXTRA_ATTRIBUTES=[
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


def filter_baseline(run):
    return ("clone:0 tokens, dfp, no cp" in run["algorithm"] 
        or "clone:0 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, dfp, offline cp" in run["algorithm"]
        or "clone:0 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, dfp, online cp" in run["algorithm"]
        or "clone:0 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, no cp" in run["algorithm"] 
        or "clone:0 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, offline cp" in run["algorithm"]
        or "clone:0 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, online cp" in run["algorithm"]
        or "clone:0 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_15_tokens(run):
    return ("clone:15 tokens, dfp, no cp" in run["algorithm"] 
        or "clone:15 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, dfp, offline cp" in run["algorithm"]
        or "clone:15 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, dfp, online cp" in run["algorithm"]
        or "clone:15 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, no cp" in run["algorithm"] 
        or "clone:15 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, offline cp" in run["algorithm"]
        or "clone:15 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, online cp" in run["algorithm"]
        or "clone:15 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_50_tokens(run):
    return ("clone:50 tokens, dfp, no cp" in run["algorithm"] 
        or "clone:50 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, dfp, offline cp" in run["algorithm"]
        or "clone:50 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, dfp, online cp" in run["algorithm"]
        or "clone:50 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, no cp" in run["algorithm"] 
        or "clone:50 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, offline cp" in run["algorithm"]
        or "clone:50 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, online cp" in run["algorithm"]
        or "clone:50 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_100_tokens(run):
    return ("clone:0 tokens, dfp, no cp" in run["algorithm"] 
        or "clone:100 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, dfp, offline cp" in run["algorithm"]
        or "clone:100 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, dfp, online cp" in run["algorithm"]
        or "clone:100 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, no cp" in run["algorithm"] 
        or "clone:100 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, offline cp" in run["algorithm"]
        or "clone:100 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, online cp" in run["algorithm"]
        or "clone:100 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

exp = FastDownwardExperiment("combined")

exp.add_fetcher("data/2023-08-21-adhoc-cloning-eval", merge=True)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_baseline
    ),
    name="baseline-0-tokens",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_15_tokens
    ),
    name="15-tokens",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_50_tokens
    ),
    name="50-tokens",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_100_tokens
    ),
    name="100-tokens",
)

exp.run_steps()
