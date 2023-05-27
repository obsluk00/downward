#! /usr/bin/env python3

import itertools
import os

from lab.environments import LocalEnvironment, BaselSlurmEnvironment
from lab.reports import Attribute, arithmetic_mean, geometric_mean

from downward.reports.compare import ComparativeReport

import common_setup
from common_setup import IssueConfig, IssueExperiment

DIR = os.path.dirname(os.path.abspath(__file__))
SCRIPT_NAME = os.path.splitext(os.path.basename(__file__))[0]
BENCHMARKS_DIR = os.environ['DOWNWARD_BENCHMARKS']
REVISION='cb6cd8738856fc97004f069b5ea4a2d7ee7c0a7c'
REVISIONS = [REVISION]
TIME_LIMIT=1800
if common_setup.is_test_run():
    DIV_TIME_LIMIT=10
CONFIGS = [
    ## atomic no lr
    IssueConfig('b50k-t900-sccdfp-atomicnolrocp', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance(),dfp(),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=0,cost_partitioning=ocp(efficient_cp=true),compute_atomic_snapshot=true,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=0,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=false))'], driver_options=['--search-time-limit', f'{DIV_TIME_LIMIT}s']),

    ## atomic lr
    IssueConfig('b50k-t900-sccdfp-atomiclrocp', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance(),dfp(),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=0,cost_partitioning=ocp(efficient_cp=true),compute_atomic_snapshot=true,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=0,atomic_label_reduction=true,snapshot_moment=after_label_reduction,filter_trivial_factors=false))'], driver_options=['--search-time-limit', f'{DIV_TIME_LIMIT}s']),

    ## interleaved snapshots - canonical (and best) choice: random order, afterlr, i1
    IssueConfig('b50k-t900-sccdfp-interleavedocp-rnd-i1-afterlr', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance(),dfp(),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=ocp(efficient_cp=true),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=1,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=false,offline_cps=false))'], driver_options=['--search-time-limit', f'{DIV_TIME_LIMIT}s']),

    ## offline - single (order: random, even though not best config in paper)
    IssueConfig('b50k-t900-sccdfp-offlineocp-rnd-i1-afterlr', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_sccs(order_of_sccs=topological,merge_selector=score_based_filtering(scoring_functions=[goal_relevance(),dfp(),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=ocp(efficient_cp=true),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=1,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=false,offline_cps=true))'], driver_options=['--search-time-limit', f'{DIV_TIME_LIMIT}s']),
]

SUITE = common_setup.DEFAULT_OPTIMAL_SUITE
ENVIRONMENT = BaselSlurmEnvironment(
    partition="infai_3",
    email="silvan.sievers@unibas.ch",
    memory_per_cpu="3940M",
    cpus_per_task=2,
    export=["PATH"],
    # paths obtained via:
    # module purge
    # module -q load Python/3.10.4-GCCcore-11.3.0
    # module -q load GCC/11.3.0
    # module -q load CMake/3.23.1-GCCcore-11.3.0
    # echo $PATH
    # echo $LD_LIBRARY_PATH
    setup='export PATH=/scicore/soft/apps/CMake/3.23.1-GCCcore-11.3.0/bin:/scicore/soft/apps/libarchive/3.6.1-GCCcore-11.3.0/bin:/scicore/soft/apps/cURL/7.83.0-GCCcore-11.3.0/bin:/scicore/soft/apps/Python/3.10.4-GCCcore-11.3.0/bin:/scicore/soft/apps/OpenSSL/1.1/bin:/scicore/soft/apps/XZ/5.2.5-GCCcore-11.3.0/bin:/scicore/soft/apps/SQLite/3.38.3-GCCcore-11.3.0/bin:/scicore/soft/apps/Tcl/8.6.12-GCCcore-11.3.0/bin:/scicore/soft/apps/ncurses/6.3-GCCcore-11.3.0/bin:/scicore/soft/apps/bzip2/1.0.8-GCCcore-11.3.0/bin:/scicore/soft/apps/binutils/2.38-GCCcore-11.3.0/bin:/scicore/soft/apps/GCCcore/11.3.0/bin:/infai/sieverss/repos/bin:/infai/sieverss/local:/export/soft/lua_lmod/centos7/lmod/lmod/libexec:/usr/local/bin:/usr/bin:/usr/local/sbin:/usr/sbin:$PATH\nexport LD_LIBRARY_PATH=/scicore/soft/apps/libarchive/3.6.1-GCCcore-11.3.0/lib:/scicore/soft/apps/cURL/7.83.0-GCCcore-11.3.0/lib:/scicore/soft/apps/Python/3.10.4-GCCcore-11.3.0/lib:/scicore/soft/apps/OpenSSL/1.1/lib:/scicore/soft/apps/libffi/3.4.2-GCCcore-11.3.0/lib64:/scicore/soft/apps/GMP/6.2.1-GCCcore-11.3.0/lib:/scicore/soft/apps/XZ/5.2.5-GCCcore-11.3.0/lib:/scicore/soft/apps/SQLite/3.38.3-GCCcore-11.3.0/lib:/scicore/soft/apps/Tcl/8.6.12-GCCcore-11.3.0/lib:/scicore/soft/apps/libreadline/8.1.2-GCCcore-11.3.0/lib:/scicore/soft/apps/ncurses/6.3-GCCcore-11.3.0/lib:/scicore/soft/apps/bzip2/1.0.8-GCCcore-11.3.0/lib:/scicore/soft/apps/binutils/2.38-GCCcore-11.3.0/lib:/scicore/soft/apps/zlib/1.2.12-GCCcore-11.3.0/lib:/scicore/soft/apps/GCCcore/11.3.0/lib64')

if common_setup.is_test_run():
    SUITE = IssueExperiment.DEFAULT_TEST_SUITE
    ENVIRONMENT = LocalEnvironment(processes=4)

exp = IssueExperiment(
    revisions=REVISIONS,
    configs=CONFIGS,
    environment=ENVIRONMENT,
)
exp.add_suite(BENCHMARKS_DIR, SUITE)

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)

exp.add_parser('ms-parser.py')
exp.add_parser('scp-ms-parser.py')
exp.add_parser('cp-ms-parser.py')

exp.add_step('build', exp.build)
exp.add_step('start', exp.start_runs)
exp.add_fetcher(name='fetch')

extra_attributes=[
    Attribute('search_out_of_memory', absolute=True, min_wins=True),
    Attribute('search_out_of_time', absolute=True, min_wins=True),
    Attribute('ms_construction_time', absolute=False, min_wins=True, function=geometric_mean),
    Attribute('score_ms_construction_time', min_wins=False, digits=4),
    Attribute('ms_atomic_construction_time', absolute=False, min_wins=True, function=geometric_mean),
    Attribute('ms_abstraction_constructed', absolute=True, min_wins=False),
    Attribute('ms_atomic_fts_constructed', absolute=True, min_wins=False),
    Attribute('ms_out_of_memory', absolute=True, min_wins=True),
    Attribute('ms_out_of_time', absolute=True, min_wins=True),
    Attribute('ms_memory_delta', absolute=False, min_wins=True),
    Attribute('ms_reached_time_limit', absolute=False, min_wins=True),

    Attribute('ms_interleaved_cps_num_cps', absolute=True, min_wins=True),
    Attribute('ms_interleaved_cps_num_abs_per_cp', absolute=True, min_wins=True),
    Attribute('ms_offline_cps_num_abs', absolute=True, min_wins=True),
    Attribute('ms_label_group_infinite_hvalue', absolute=True, min_wins=True),
    Attribute('ms_dead_label_group', absolute=True, min_wins=True),
    Attribute('ms_scps_num_cps', absolute=True, min_wins=True),
]
attributes = list(exp.DEFAULT_TABLE_ATTRIBUTES)
attributes.extend(extra_attributes)

exp.add_absolute_report_step(
    filter_algorithm=[
        '{}-b50k-t900-sccdfp-atomicnolrocp'.format(REVISION),
        '{}-b50k-t900-sccdfp-atomiclrocp'.format(REVISION),
        '{}-b50k-t900-sccdfp-interleavedocp-rnd-i1-afterlr'.format(REVISION),
        '{}-b50k-t900-sccdfp-offlineocp-rnd-i1-afterlr'.format(REVISION),
    ],
    attributes=attributes,
)

exp.run_steps()
