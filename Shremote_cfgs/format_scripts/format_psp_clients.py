#!/usr/bin/env python3
import yaml
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument('output')

parser.add_argument('--client-ips', nargs='+')
parser.add_argument('--client-macs', nargs='+')
parser.add_argument('--clt-threads', type=int)
parser.add_argument('--server-ip', type=str)
parser.add_argument('--server-mac', type=str)
parser.add_argument('--server-workers', type=int)
parser.add_argument('--port', type=int, default=6789)
parser.add_argument('--dev-id', type=int, default=0)
parser.add_argument('--log-dir', type=str, default=None)
parser.add_argument('--dpdk-dev-num', type=str, default="18:00.0")
parser.add_argument('--dpdk-prefix', type=str, default='dpdk')
parser.add_argument('--cpus', type=str, nargs='+', default='1')
parser.add_argument('--schedule', type=str)
parser.add_argument('--load', type=float)

args = parser.parse_args()

n_clients = len(args.client_ips)

if args.schedule:
    schedules = []
    with open(args.schedule, 'r') as f:
        workloads = yaml.load(f, Loader=yaml.FullLoader)
    for workload in workloads:
        schedule = {}
        assert(len(workload['mean_ns']) == len(workload['ratios']))
        schedule['rate'] = int(
            ((1e9
                /
             (sum([u * r for u, r in zip(workload['mean_ns'], workload['ratios'])]) / args.server_workers)
            )
            / n_clients)
            * args.load
        )
        print(f"rate per client: {schedule['rate']}")
        schedule['cmd_mean_ns'] = workload['mean_ns']
        schedule['cmd_ratios'] = workload['ratios']
        schedule['uniform'] = workload['uniform']
        schedule['duration'] = workload['duration']
        schedule['ptype'] = workload['ptype']
        schedules.append(schedule)

client_cpus = ', '.join(['0'] + [cpu for cpu in args.cpus])
for i, (ip, mac) in enumerate(zip(args.client_ips, args.client_macs)):
    config = {
        'network': {
            'mac': mac,
            'remote_mac': args.server_mac,
            'device_id': args.dev_id,
            'eal_init': [
                '-n', '2',
                '-l', client_cpus,
                '--file-prefix', args.dpdk_prefix,
                "-w", "{}".format(args.dpdk_dev_num),
                #"-w", "{},mprq_en=0,rx_vec_en=0".format(args.dpdk_dev_num),
            ]
        },
        'cpus': [int(cpu) for cpu in args.cpus],
    }
    if args.log_dir:
        config['log_dir'] = args.log_dir
    config['net_workers'] = [{
        'ip': ip,
        'port': args.port,
    }] * args.clt_threads

    if args.schedule:
        config['schedules'] = schedules

    yaml.dump(config, open(args.output + '.' + str(i), 'w'))
