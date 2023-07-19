#! /usr/bin/env python

from lab.parser import Parser

parser = Parser()

parser.add_pattern('ms_interleaved_cps_num_cps', 'Interleaved CPs: number of CPs: (\d+)', required=False, type=int)
parser.add_pattern('ms_interleaved_cps_num_abs_per_cp', 'Interleaved CPs: average number of abstractions per CP: (.+)', required=False, type=float)
parser.add_pattern('ms_offline_cps_num_abs', 'Offline CPs: number of abstractions: (\d+)', required=False, type=int)

parser.parse()
