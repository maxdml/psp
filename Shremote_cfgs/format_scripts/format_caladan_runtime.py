#!/usr/bin/env python3
import yaml
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument('output')

#server templates arguments
parser.add_argument('--server-ip', type=str)
parser.add_argument('--server-mac', type=str)
parser.add_argument('--client-ips', type=str, nargs='+')
parser.add_argument('--client-macs', type=str, nargs='+')
parser.add_argument('--directpath', type=int, default=1)
parser.add_argument('--runtime-kthreads', type=int, default=8)
parser.add_argument('--runtime-guaranteed-kthreads', type=int, default=8)
parser.add_argument('--runtime-spinning-kthreads', type=int, default=8)

args = parser.parse_args()

config = f"host_addr {args.server_ip}\n"
config += f"host_netmask 255.255.255.0\n"
config += f"host_gateway 192.168.1.1\n"
config += f"runtime_kthreads {args.runtime_kthreads}\n"
config += f"runtime_guaranteed_kthreads {args.runtime_guaranteed_kthreads}\n"
config += f"runtime_spinning_kthreads {args.runtime_spinning_kthreads}\n"
config += f"enable_directpath {args.directpath}\n"
config += f"host_mac {args.server_mac}\n"
for ip, mac in zip(args.client_ips, args.client_macs):
    config += f"static_arp {ip} {mac}\n"

with open(args.output, 'w') as f:
    f.write(config)
