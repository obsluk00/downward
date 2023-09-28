#! /usr/bin/env python3

from downward.experiment import FastDownwardExperiment
from downward.reports import Attribute
from downward.reports.absolute import AbsoluteReport
from lab.reports import geometric_mean, arithmetic_mean, Report

non_orthogonality = Attribute("non_orthogonality", absolute=True)
times_cloned = Attribute("times_cloned", absolute=True)
average_cloned = Attribute("average_cloned", absolute=True)
largest_cloned = Attribute("largest_cloned", absolute=True)
ATTRIBUTES = [
"coverage",
"error",
"planner_time",
"quality",
"score_evaluations",
"score_expansions",
"score_generated",
"score_memory",
"score_search_time",
"score_total_time",
"search_time",
"total_time"
]

EXTRA_ATTRIBUTES=[
     Attribute('search_out_of_memory', absolute=True, min_wins=True),
     Attribute('search_out_of_time', absolute=True, min_wins=True),
     Attribute('ms_construction_time', absolute=False, min_wins=True,
function=geometric_mean),
     Attribute('score_ms_construction_time', min_wins=False, digits=4),
     Attribute('ms_out_of_memory', absolute=True, min_wins=True),
     Attribute('ms_out_of_time', absolute=True, min_wins=True)
]


def filter_depth_0(run):
    return ("depth 0, offline CP" in run["algorithm"]
        or "depth 0, online CP" in run["algorithm"])

def filter_no_offline_cp(run):
    return ("clustering [pre_eff, eff_eff], combining combine_smallest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 1,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 1,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 1,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 2,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 2,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 2,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 4,15 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 4,50 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_smallest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_smallest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_smallest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining combine_largest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining combine_largest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining combine_largest, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_eff], combining random, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [eff_pre, eff_eff], combining random, depth 4,100 tokens, offline CP: false" in run["algorithm"]
        or "clustering [pre_eff, eff_pre, eff_eff], combining random, depth 4,100 tokens, offline CP: false" in run["algorithm"])



exp = FastDownwardExperiment("precomputed-combined")

exp.add_fetcher("data/2023-09-18-precomputed-cloning-eval", merge=True)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_depth_0
    ),
    name="orthogonal-baseline",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_no_offline_cp
    ),
    name="no_offline_cp",
)

exp.run_steps() 
