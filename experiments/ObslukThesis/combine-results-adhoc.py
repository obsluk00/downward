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


def filter_baseline(run):
    return ("clone:0 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:0 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_15_tokens(run):
    return ("clone:15 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:15 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_50_tokens(run):
    return ("clone:50 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:50 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

def filter_100_tokens(run):
    return ("clone:100 tokens, dfp, no cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, dfp, offline cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, dfp, online cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, no cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, offline cp, avoid_existing" in run["algorithm"]
        or "clone:100 tokens, miasm, online cp, avoid_existing" in run["algorithm"])

exp = FastDownwardExperiment("combined")

exp.add_fetcher("data/2023-08-21-adhoc-cloning-eval", merge=True)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_baseline
    ),
    name="baseline-0-tokens-avoid-existing",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_15_tokens
    ),
    name="15-tokens-avoid-existing",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_50_tokens
    ),
    name="50-tokens-avoid-existing",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_100_tokens
    ),
    name="100-tokens-avoid-existing",
)

exp.add_report(
    AbsoluteReport(
        attributes=ATTRIBUTES + EXTRA_ATTRIBUTES, filter=filter_baseline, format="tex"
    )
)

exp.run_steps()
