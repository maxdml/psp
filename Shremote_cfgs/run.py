#!/usr/bin/env python3

##############################################
####################LOGGER####################
import logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

ch = logging.StreamHandler()
ch.setLevel(logging.INFO)

formatter = logging.Formatter('%(levelname)s: %(message)s')
ch.setFormatter(formatter)

logger.addHandler(ch)

def log_error(*args, **kwargs):
    logger.error(*args, **kwargs)

def log_info(*args, **kwargs):
    logger.info(*args, **kwargs)

def log_warn(*args, **kwargs):
    logger.warn(*args, **kwargs)

def log_debug(*args, **kwargs):
    logger.debug(*args, **kwargs)
##############################################

import yaml
import subprocess
import numpy as np
import argparse
import os
parser = argparse.ArgumentParser()
parser.add_argument('run_number')
parser.add_argument('system')
parser.add_argument('schedule')
parser.add_argument('-p', '--policies', nargs='+', type=str, default='DARC')
parser.add_argument('-w', '--n-workers', type=str, default='14')
parser.add_argument('-c', '--n-clients', type=str, default='1')
parser.add_argument('-C', '--n-clt-threads', type=str, default='1')
parser.add_argument('-T', '--parse-test', dest='parse_test', action='store_true')
parser.add_argument('-a', '--app-type', type=str, default='MB')
parser.add_argument('-d', '--downsample', type=int, default=-1)
parser.add_argument('-b', '--base-output', type=str, default='../experiments')
# env
args = parser.parse_args()
DIR = os.path.dirname(os.path.abspath(__file__))
SCHEDULES = os.path.join(DIR, "schedules", "")
SHREMOTE = os.path.join(DIR, "..", "submodules", "Shremote/shremote.py")
BASE_OUTPUT = args.base_output
SRV_CPUS = "2 4 6 8 10 12 14 16 18 20 22 24 26 28 30"
CLT_CPUS = "2"
if isinstance(args.policies, str):
    args.policies = [args.policies]
if args.system == 'shinjuku':
    SRV_CPUS = "2 34 4 6 8 10 12 14 16 18 20 22 24 26 28 30"
    CFG = os.path.join(DIR, "shinjuku.yml")
elif args.system == 'caladan':
    CFG = os.path.join(DIR, "caladan.yml")
else:
    CFG = os.path.join(DIR, "psp.yml")
output_paths = []
for LOAD in np.arange(.05, 1.06, .05):
#for LOAD in [0.80]:
    for DP in args.policies:
        TITLE = f'{DP}_{(LOAD):.2f}_{args.schedule}_{args.n_workers}.{args.run_number}'
        shremote_args = [
            'python3', '-u',
            SHREMOTE, CFG, TITLE, '--out', BASE_OUTPUT, '--delete', '--',
            '--downsample', str(args.downsample),
            '--n-clients', args.n_clients,
            '--clt-cpus', CLT_CPUS,
            '--max-clt-cc', '-1',
            '--clt-threads', args.n_clt_threads,
            '--n-workers', args.n_workers,
            '--srv-cpus', SRV_CPUS,
            '--app', args.app_type,
            '--srv-dp', DP,
            '--schedule', f'{SCHEDULES}{args.schedule}.yml',
            '--load', str(LOAD)
        ]
        if args.system == 'shinjuku':
                if args.schedule == 'TPCC' and DP == 'c-PRE-MQ':
                    shremote_args.extend(['--n-ports', '5', '--preemption-tick', '15000'])
                elif DP == 'c-PRE-MQ':
                    shremote_args.extend(['--n-ports', '2', '--preemption-tick', '15000'])
                elif DP == 'c-PRE-SQ':
                    shremote_args.extend(['--n-ports', '1', '--preemption-tick', '15000'])
        if args.parse_test:
            shremote_args.append('--parse-test')
        log_info(shremote_args)
        p = subprocess.Popen(shremote_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        while(1):
            line = p.stdout.readline()
            print(line.decode('ascii'))
            if not line:
                break
        p.wait()
        output_paths.append(f'{BASE_OUTPUT}/{TITLE}')

for p in output_paths:
    print(p)
