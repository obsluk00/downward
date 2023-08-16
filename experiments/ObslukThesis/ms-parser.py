#! /usr/bin/env python

import math

from lab.parser import Parser

parser = Parser()
# parser.add_pattern('ms_construction_time', 'Done initializing merge-and-shrink heuristic \[(.+)s\]', required=False, type=float)
parser.add_pattern('ms_construction_time', 'Merge-and-shrink algorithm runtime: (.+)s', required=False, type=float)
parser.add_pattern('ms_atomic_construction_time', 'M&S algorithm timer: (.+)s \(after computation of atomic factors\)', required=False, type=float)
# parser.add_pattern('ms_atomic_construction_time', 't=(.+)s \(after computation of atomic transition systems\)', required=False, type=float)
# parser.add_pattern('ms_memory_delta', 'Final peak memory increase of merge-and-shrink computation: (\d+) KB', required=False, type=int)
parser.add_pattern('ms_memory_delta', 'Final peak memory increase of merge-and-shrink algorithm: (\d+) KB', required=False, type=int)
parser.add_pattern('ms_main_loop_max_time', 'Main loop max time in seconds: (\d+)', required=False, type=int)

def check_ms_constructed(content, props):
    ms_construction_time = props.get('ms_construction_time')
    abstraction_constructed = False
    if ms_construction_time is not None:
        abstraction_constructed = True
    props['ms_abstraction_constructed'] = abstraction_constructed

parser.add_function(check_ms_constructed)

def check_atomic_fts_constructed(content, props):
    ms_atomic_construction_time = props.get('ms_atomic_construction_time')
    ms_atomic_fts_constructed = False
    if ms_atomic_construction_time is not None:
        ms_atomic_fts_constructed = True
    props['ms_atomic_fts_constructed'] = ms_atomic_fts_constructed

parser.add_function(check_atomic_fts_constructed)

def check_planner_exit_reason(content, props):
    ms_abstraction_constructed = props.get('ms_abstraction_constructed')
    error = props.get('error')
    if error != 'success' and error != 'search-out-of-time' and error != 'search-out-of-memory':
        print('error: %s' % error)
        return

    # Check whether merge-and-shrink computation or search ran out of
    # time or memory.
    ms_out_of_time = False
    ms_out_of_memory = False
    search_out_of_time = False
    search_out_of_memory = False
    if ms_abstraction_constructed == False:
        if error == 'search-out-of-time':
            ms_out_of_time = True
        elif error == 'search-out-of-memory':
            ms_out_of_memory = True
    elif ms_abstraction_constructed == True:
        if error == 'search-out-of-time':
            search_out_of_time = True
        elif error == 'search-out-of-memory':
            search_out_of_memory = True
    props['ms_out_of_time'] = ms_out_of_time
    props['ms_out_of_memory'] = ms_out_of_memory
    props['search_out_of_time'] = search_out_of_time
    props['search_out_of_memory'] = search_out_of_memory

parser.add_function(check_planner_exit_reason)

def check_reached_time_limit(content, props):
    props['ms_reached_time_limit'] = False
    for line in content.splitlines():
        if line == 'Ran out of time, stopping computation.':
            props['ms_reached_time_limit'] = True

parser.add_function(check_reached_time_limit)

def add_construction_time_score(content, props):
    """
    Convert ms_construction_time into scores in the range [0, 1].

    Best possible performance in a task is counted as 1, while failure
    to construct the heuristic and worst performance are counted as 0.

    """
    def log_score(value, min_bound, max_bound):
        if value is None:
            return 0
        value = max(value, min_bound)
        value = min(value, max_bound)
        raw_score = math.log(value) - math.log(max_bound)
        best_raw_score = math.log(min_bound) - math.log(max_bound)
        return raw_score / best_raw_score

    main_loop_max_time = props.get('ms_main_loop_max_time')
    if main_loop_max_time is not None and main_loop_max_time == float('inf'):
        max_time = props.get('limit_search_time')
        if max_time is not None:
            main_loop_max_time = max_time
    if main_loop_max_time is not None and main_loop_max_time != 0 and main_loop_max_time != float('inf'):
        props['score_ms_construction_time'] = log_score(props.get('ms_construction_time'), min_bound=1.0, max_bound=main_loop_max_time)

parser.add_function(add_construction_time_score)

parser.parse()
