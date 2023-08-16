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
OTHER_REV = '9fecd461ade4532fc1739fc9e7f432f1f52cdc28'
REVISION = '7bc50275fd3a6e03a636101403c32ebb7bde70e7'
REVISIONS = [REVISION]
DIV_TIME_LIMIT=200
if common_setup.is_test_run():
    DIV_TIME_LIMIT=2
CONFIGS = [
    ## final snapshot
    IssueConfig('b50k-t900-cp-interleavedscp-rnd-afterlr', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_cp(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),filter_trivial_factors=false),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=0,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=true,offline_cps=false))']),

    ## interleaved snapshots - canonical (and best) choice: random order, afterlr, i1
    IssueConfig('b50k-t900-cp-interleavedscp-rnd-i1-afterlr', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_cp(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),filter_trivial_factors=false),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=1,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=true,offline_cps=false))']),

    ## offline - single (order: random, even though not best config in paper)
    IssueConfig('b50k-t900-cp-offlinescp-rnd-i1-afterlr', ['--search', 'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_cp(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),filter_trivial_factors=false),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=1,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=true,offline_cps=true))']),

    ## offline - many (opt + div) (order: random, even though not best config in paper)
    IssueConfig('b50k-t900-cp-offlinescps-rnd-i1-afterlr', ['--search', f'astar(max_cp_ms(shrink_strategy=shrink_bisimulation(greedy=false),label_reduction=exact(before_shrinking=true,before_merging=false),merge_strategy=merge_stateless(merge_selector=score_based_filtering(scoring_functions=[sf_cp(shrink_strategy=shrink_bisimulation(greedy=false),max_states=50000,cost_partitioning=scp(order_generator=random_orders(random_seed=2020)),filter_trivial_factors=false),total_order(atomic_ts_order=reverse_level,product_ts_order=new_to_old,atomic_before_product=false)])),max_states=50K,threshold_before_merge=1,main_loop_max_time=900,cost_partitioning=scps(order_generator=random_orders(random_seed=2020),max_orders=infinity,max_optimization_time=2,diversify=true,max_time={DIV_TIME_LIMIT}),compute_atomic_snapshot=false,main_loop_target_num_snapshots=0,main_loop_snapshot_each_iteration=1,atomic_label_reduction=false,snapshot_moment=after_label_reduction,filter_trivial_factors=true,offline_cps=true))']),
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
        '{}-b50k-t900-cp-interleavedscp-rnd-afterlr'.format(REVISION),
        '{}-b50k-t900-cp-interleavedscp-rnd-i1-afterlr'.format(REVISION),
        '{}-b50k-t900-cp-offlinescp-rnd-i1-afterlr'.format(REVISION),
        '{}-b50k-t900-cp-offlinescps-rnd-i1-afterlr'.format(REVISION),
    ],
    attributes=attributes,
)

exp.add_fetcher(
    'data/2023-05-26-sccdfp-scp-eval',
    filter_algorithm=[
        '{}-b50k-t900-sccdfp-interleavedscp-rnd-afterlr'.format(OTHER_REV),
        '{}-b50k-t900-sccdfp-interleavedscp-rnd-i1-afterlr'.format(OTHER_REV),
        '{}-b50k-t900-sccdfp-offlinescp-rnd-i1-afterlr'.format(OTHER_REV),
        '{}-b50k-t900-sccdfp-offlinescps-rnd-i1-afterlr'.format(OTHER_REV),
    ],
    merge=True
)

compared_configs = [
    ('{}-b50k-t900-sccdfp-interleavedscp-rnd-afterlr'.format(OTHER_REV), '{}-b50k-t900-cp-interleavedscp-rnd-afterlr'.format(REVISION)),
    ('{}-b50k-t900-sccdfp-interleavedscp-rnd-i1-afterlr'.format(OTHER_REV),
    '{}-b50k-t900-cp-interleavedscp-rnd-i1-afterlr'.format(REVISION)),
    ('{}-b50k-t900-sccdfp-offlinescp-rnd-i1-afterlr'.format(OTHER_REV),
    '{}-b50k-t900-cp-offlinescp-rnd-i1-afterlr'.format(REVISION)),
    ('{}-b50k-t900-sccdfp-offlinescps-rnd-i1-afterlr'.format(OTHER_REV),
    '{}-b50k-t900-cp-offlinescps-rnd-i1-afterlr'.format(REVISION)),
]
exp.add_report(
    ComparativeReport(compared_configs, attributes=attributes),
    outfile=os.path.join(exp.eval_dir, f"{exp.name}-compare.html"),
    name='compare-report'
)

exp.run_steps()
