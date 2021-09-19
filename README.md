Perséphone
==========

Perséphone is a kernel-bypass request scheduler. It is designed to improve tail latency for short microsecond scale requests in heavy-tailed workloads. We evaluate Perséphone against two competing systems, [Shenango](https://www.usenix.org/conference/nsdi19/presentation/ousterhout) and [Shinjuku](https://www.usenix.org/conference/nsdi19/presentation/kaffes).


Setting up Perséphone
=====================

```bash
# Install required packages.
sudo apt-get update && sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" upgrade -y; sudo apt install -y cmake libaio-dev libcunit1-dev libjemalloc-dev libmnl-dev libnl-3-dev libnl-route-3-dev libboost-program-options-dev libboost-system-dev libboost-chrono-dev libboost-context-dev libnuma-dev libyaml-cpp-dev liblz4-dev libgflags-dev libsnappy-dev libbz2-dev libzstd-dev numactl msr-tools htop libconfig-dev software-properties-common; sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test; sudo apt update; sudo apt install -y gcc-7 g++-7; sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-7
export PSP_DIR=~/Persephone
```
To enable more precise measurements, take the first NUMA node out of CFS' domain and disable kaslr.
In _/etc/default/grub_, append the following line to the entry "GRUB_CMDLINE_LINUX_DEFAULT"
> nokaslr isolcpus=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 nohz=on nohz_full=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 maxcpus=64
Reboot all the nodes:
```bash
# Update grub to apply these changes
sudo update-grub; sudo reboot
```
Check whether the change was correctly applied:
```bash
cat /proc/cmdline
BOOT_IMAGE=/boot/vmlinuz-4.4.0-210-generic root=UUID=ce184cb1-3771-4a20-b6cd-8e9a4649a561 ro console=ttyS0,115200 nokaslr isolcpus=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 nohz=on nohz_full=0,2,4,6,8,10,,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 maxcpus=64
export PSP_DIR=~/Persephone
```

Clone the repo on the server machine and run the setup script.
```bash
git clone --recurse-submodules https://github.com/maxdml/psp.git ${PSP_DIR}
# set `-DDPDK_MELLANOX_SUPPORT` to `ON` for Connectx-5
${PSP_DIR}/scripts/setup/base_setup.sh
```

Simple client-server tests
========================

On the server On the server (reboot on 4.4.0-187-generic if needed):
```bash
${PSP_DIR}/scripts/setup/base_start.sh
cd ${PSP_DIR}/build/src/c++/apps/app/
sudo numactl -N0 -m0 ./psp-app --cfg ${PSP_DIR}/configs/base_psp_cfg.yml --label test
```

On one client:
```bash
sudo numactl -N0 -m0 ${PSP_DIR}/client/build/src/c++/apps//client/client --config-path ${PSP_DIR}/configs/base_client_psp_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
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

Running experiments
=======================
We use [Shremote](Shremote) to orchestrate experiments and gather results. We also use a [notebook](scripts/experiments/psp.pynb) to plot the figures. The notebook includes sample shremote commands. For convenience you can setup a docker container using our provided [Dockerfile](scripts/experiments/Dockerfile).

Setting up The container
----------------

[Install docker](https://docs.docker.com/engine/install/ubuntu/)  (you might have to restart the docker service before running a container).

Build and start the container:
```bash
cd ${PSP_DIR}/scripts/experiments/
sudo docker build -t psp .
sudo docker run -p 8888:8888 psp
```
Then:
- Log in the container to configure it `bash sudo docker exec -it CONTAINER_ID /bin/bash`
- You can find the docker container ID with `sudo docker ps`
- Setup private SSH key for the machines hosting your server and clients
- In /psp/Shremote_cfgs/config, update:
    - ssh_config.yml: set your remote username
    - hosts.yml: update "addr" for each machine
- In scripts/experiments/loader.py, update "exp_base_folder" for the location of ${PSP_DIR}/experiments-data
