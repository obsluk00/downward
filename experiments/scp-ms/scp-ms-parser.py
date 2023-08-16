#! /usr/bin/env python

from lab.parser import Parser

parser = Parser()

parser.add_pattern('ms_scps_num_cps', 'Cost partitionings: (\d+)', required=False, type=int)

def parse_lines(content, props):
    dead_label_group = False
    label_group_infinite_hvalue = False
    for line in content.splitlines():
        if line == 'found dead label group':
            dead_label_group = True
        if line == 'label group does not lead to any state with finite heuristic value':
            label_group_infinite_hvalue = True
    props['ms_label_group_infinite_hvalue'] = label_group_infinite_hvalue
    props['ms_dead_label_group'] = dead_label_group

parser.add_function(parse_lines)

parser.parse()
