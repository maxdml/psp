#!/usr/bin/env python3
import yaml
from argparse import ArgumentParser
from collections import OrderedDict

parser = ArgumentParser()
parser.add_argument('output')

#server templates arguments
parser.add_argument('--server-ip', type=str)
parser.add_argument('--server-mac', type=str)
parser.add_argument('--port', type=int, default=6789)
parser.add_argument('--dev-id', type=int, default=0)
parser.add_argument('--log-dir', type=str, default=None)
parser.add_argument('--dpdk-dev-num', type=str, default="18:00.0")
parser.add_argument('--dpdk-prefix', type=str, default='dpdk')
parser.add_argument('--cpus', type=str, nargs='+', default='1')
parser.add_argument('--dp-pol', type=str, default='DFCFS')
parser.add_argument('--app-type', type=str, default='MB')
parser.add_argument('--req-type', nargs=4, action='append')
parser.add_argument('--n-resas', type=int, default=-1)
parser.add_argument('--schedule', type=str)

args = parser.parse_args()

config = {
    'network': {
        'device_id': args.dev_id,
        'mac': args.server_mac,
        'eal_init': [
            '-n', '2',
            '-l', ', '.join(['0'] + args.cpus), #1 master EAL + net worker + app workers
            '--file-prefix', args.dpdk_prefix,
            "-w", "{}".format(args.dpdk_dev_num),
            #"-w", "{},mprq_en=0,rx_vec_en=0".format(args.dpdk_dev_num),
        ]
    },
    'net_workers': [{
        'ip': args.server_ip,
        'port': args.port,
        'dp': args.dp_pol,
        'is_echo': 0
    }],
    'cpus': [int(cpu) for cpu in args.cpus],
    'workers': {
        'number': len(args.cpus) - 1, #Assuming split_dpt = 0
        'type': args.app_type,
    }
}
if args.n_resas > -1:
    config['n_resas'] = args.n_resas

if args.schedule:
    types = []
    with open(args.schedule, 'r') as f:
        workloads = yaml.load(f, Loader=yaml.FullLoader)
    for workload in workloads:
        types.extend(workload['rtype'])
    unique_types = list(OrderedDict.fromkeys(types))
    config['requests'] = [{'type': r} for r in unique_types]
elif args.req_type is not None:
    req_types = []
    for rtype in args.req_type:
        req_types.append({
            'type': rtype[0],
            'mean_ns': float(rtype[1]),
            'ratio': float(rtype[2]),
            'deadline': float(rtype[3])
        })
    config['requests'] = req_types


if args.log_dir:
    config['log_dir'] = args.log_dir

yaml.dump(config, open(args.output, 'w'))
