Perséphone
==========

Perséphone is a kernel-bypass request scheduler. It is designed to improve tail latency for short microsecond scale requests in heavy-tailed workloads. It was presented at [SOSP 2021](https://dl.acm.org/doi/10.1145/3477132.3483571).

This repo also hosts the artifact we used for SOSP's artifact evaluation, in the branches "master" and "client". Please refer to the master branch to reproduce our results.

Setting up Perséphone
=====================

```bash
# Install required packages.
sudo apt-get update && sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" upgrade -y; sudo apt install -y cmake libaio-dev libcunit1-dev libjemalloc-dev libmnl-dev libnl-3-dev libnl-route-3-dev libboost-program-options-dev libboost-system-dev libboost-chrono-dev libboost-context-dev libnuma-dev libyaml-cpp-dev liblz4-dev libgflags-dev libsnappy-dev libbz2-dev libzstd-dev numactl msr-tools htop libconfig-dev software-properties-common; sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test; sudo apt update; sudo apt install -y gcc-7 g++-7; sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-7
# We use PSP_DIR across the setup scripts.
export PSP_DIR=~/Persephone
# Clone the repo on the server machine and run the setup script.
git clone --recurse-submodules https://github.com/maxdml/psp.git ${PSP_DIR}
# set `-DDPDK_MELLANOX_SUPPORT` to `ON` for Connectx-5
${PSP_DIR}/scripts/setup/base_setup.sh
```

Simple client-server tests
========================

On the server:
```bash
${PSP_DIR}/scripts/setup/base_start.sh
cd ${PSP_DIR}/build/src/c++/apps/app/
sudo numactl -N0 -m0 ./psp-app --cfg ${PSP_DIR}/configs/base_psp_cfg.yml --label test
```

On a client:
```bash
sudo numactl -N0 -m0 ${PSP_DIR}/build/src/c++/apps//client/client --config-path ${PSP_DIR}/configs/base_client_psp_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
```

If you see an output similar to the one below, the system works as expected. You can ignore the Fdir error.
> 019.987:/proj/psp-PG0/maxdml/client/src/c++/apps/client/client.hh:105:~Client(): INFO: Duration: 10.00s -> Sent: 10002462, Received: 10002462, 0 sent but not answered, 1 behind schedule,  0 skipped, 170449224 events processed, 10002462 send attempts  
>eth stats for port 0[port 0], RX-packets: 10002462 RX-dropped: 0 RX-bytes: 600147720  
>[port 0] TX-packets: 10002462 TX-bytes: 600147720  
>RX-error: 0 TX-error: 0 RX-mbuf-fail: 0  
>EXTENDED PORT STATISTICS:  
>================  
>Port 0: _______ rx_good_packets:		10002462  
>Port 0: _______ tx_good_packets:		10002462  
>Port 0: _______ rx_good_bytes:		600147720  
>Port 0: _______ tx_good_bytes:		600147720  
>Port 0: _______ rx_unicast_packets:		10002462  
>Port 0: _______ rx_unknown_protocol_packets:		10002463  
>Port 0: _______ tx_unicast_packets:		10002462  
>Port 0: _______ rx_size_64_packets:		10002463  
>Port 0: _______ tx_size_64_packets:		10002462  
>Port 0: _______ rx_flow_director_sb_match_packets:		4294967275  
