#!/usr/bin/env python3
import yaml
from argparse import ArgumentParser
from pathlib import Path

parser = ArgumentParser()
parser.add_argument('template')
parser.add_argument('output')
parser.add_argument('--client-ips', type=str, nargs='+')
parser.add_argument('--client-macs', type=str, nargs='+')
parser.add_argument('--port', type=str, nargs='+', default=['6789'])
parser.add_argument('--slo', type=str, nargs='+', default=['1000'])
parser.add_argument('--dpdk-dev-num', type=str, default="18:00.0")
parser.add_argument('--cpus', type=str, nargs='+', default='1')
parser.add_argument('--headenq', type=bool, default=False)
parser.add_argument('--preemption-tick', type=int, default=5000)
parser.add_argument('--schedule', type=str)

args = parser.parse_args()

with open(args.template, 'r') as cfg_tpl:
    cfg_str = cfg_tpl.read()

# We just need to extract deadlines for each request type. Assume only 1 schedule for now
if args.schedule:
    schedules = []
    with open(args.schedule, 'r') as f:
        workloads = yaml.load(f, Loader=yaml.FullLoader)
    for workload in workloads:
        deadlines = str([d * 10 for d in workload['mean_ns']])
        break;

with open(args.output, 'w') as cfg:
    cfg.write(cfg_str)
    cfg.write('cpu=[' + ','.join([cpu for cpu in args.cpus]) + ']')
    cfg.write('\ndevices=\"' + args.dpdk_dev_num + '\"')
    cfg.write('\npreemption_delay=' + str(args.preemption_tick))
    if len(args.port) == 1:
        cfg.write('\nport=' + args.port[0])
    else:
        cfg.write('\nport=[' + ','.join(args.port) + ']')
    if deadlines:
        cfg.write('\nslo=' + str(deadlines))
    elif len(args.slo) == 1:
        cfg.write('\nslo=' + args.slo[0])
    else:
        cfg.write('\nslo=[' + ','.join(args.slo) + ']')
    if args.headenq:
        cfg.write('\nqueue_setting=[true]')
    else:
        cfg.write('\nqueue_setting=[false]')
    # Fill ARP entries for clients
    cfg.write('\narp=(')
    for i, (ip, mac) in enumerate(zip(args.client_ips, args.client_macs)):
        if i > 0:
            cfg.write(',')
        cfg.write('\n{\n')
        cfg.write('ip : \"' + ip + '\"\n')
        cfg.write('mac : \"' + mac + '\"')
        cfg.write('\n}')
    cfg.write('\n)')
