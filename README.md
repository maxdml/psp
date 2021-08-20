Perséphone
==========

This repository hosts our artifact for the SOSP'21 Artifact Evaluation Committee.
Perséphone is a kernel-bypass request scheduler. It is designed to improve tail latency for short microsecond scale requests in heavy-tailed workloads. We evaluate Perséphone against two competing systems, [Shenango](https://www.usenix.org/conference/nsdi19/presentation/ousterhout) and [Shinjuku](https://www.usenix.org/conference/nsdi19/presentation/kaffes).

We will use cloudlab to reproduce the paper's results. Because Shinjuku requires a specific version of Linux 4.4.0, we break down the evaluation in two steps: gathering and plotting results for Shinjuku and Perséphone first, then gathering data for Shenango and updating the plots.

We estimate the entire process to take X amount of time.

Setting up Perséphone
=====================

Creating the cloudlab experiment
--------------------------------
We will use Clemson's c6420 machines, so make sure that some are [available](https://www.cloudlab.us/resinfo.php).

- [Login to Cloudlab](https://www.cloudlab.us/login.php).
- [Create a new experiment profile](https://www.cloudlab.us/manage_profile.php).
- Upload [this profile](sosp_aec/cloudlab.py)

Now, instantiate a new experiment using the profile. You should be able to login using your cloudlab credentials.

Building the systems
---------------------------------
On the server machine:
```bash
export AE_DIR=/usr/local/sosp
git clone --recurse-submodules https://github.com/maxdml/psp.git ${AE_DIR}/Persephone
${AE_DIR}/Persephone/sosp_aec/base_setup.sh
```
This script will setup Perséphone, Shinjuku, and other dependent systems.

Shenango builds and runs on a different kernel, so we need to configure and restart the machine:
```bash
sudo ${AE_DIR}/Persephone/scripts/setup/pick_kernel.sh 4.15.0-142-generic
sudo reboot
```
Then:
```bash
export AE_DIR=/usr/local/sosp
${AE_DIR}/Persephone/sosp_aec/build_shenango.sh
```

On the client machines:
```bash
export AE_DIR=/usr/local/sosp
git clone --recurse-submodules https://github.com/maxdml/psp.git ${AE_DIR}/client; cd ${AE_DIR}/client; git checkout client; mkdir ${AE_DIR}/client/build; cd ${AE_DIR}/client/build; cmake -DCMAKE_BUILD_TYPE=Release -DDPDK_MELLANOX_SUPPORT=OFF ${AE_DIR}/client; make -j -C ${AE_DIR}/client/build
${AE_DIR}/client/sosp_aec/base_start.sh client
mkdir /dev/shm/experiments
```

Simple client-server tests
---------------------------------
We will now make sure that all three systems work (Perséphone, Shinjuku, Shenango).

### Perséphone
On the server On the server (reboot on 4.4.0-187-generic if needed):
```bash
${AE_DIR}/Persephone/sosp_aec/base_start.sh Persephone
cd ${AE_DIR}/Persephone/build/src/c++/apps/app/
sudo numactl -N0 -m0 ./psp-app --cfg ${AE_DIR}/Persephone/sosp_aec/configs/base_psp_cfg.yml --label test
```

On one client:
```bash
sudo numactl -N0 -m0 ${AE_DIR}/client/build/src/c++/apps//client/client --config-path ${AE_DIR}/client/sosp_aec/configs/base_client_psp_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
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

### Shinjuku
On the server (reboot on 4.4.0-187-generic if needed)
```bash
# We use base_start.sh to unbind the NIC from igb_uio
${AE_DIR}/Persephone/sosp_aec/base_start.sh shinjuku
sudo numactl -N0 -m0 ${AE_DIR}/Persephone/submodules/shinjuku/shinjuku -c ${AE_DIR}/Persephone/sosp_aec/configs/base_shinjuku_conf
```

On the client update the server's NIC MAC address in the config file.
One way to find the NIC MAC ID is through the Cloudlab portal, by clicking on the node. It should be the only 10Gbps NIC, eth1.
In ${AE_DIR}/client/sosp_aec/configs/base_client_sjk_cfg.yml, put that value in the field "remote_mac".
```bash
sudo numactl -N0 -m0 ${AE_DIR}/client/build/src/c++/apps//client/client --config-path ${AE_DIR}/client/sosp_aec/configs/base_client_sjk_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
```

You should have a similar ouput than for Perséphone if this test worked correctly.

### Shenango
On one server terminal (reboot on 4.15.0-142-generic if needed)
```bash
${AE_DIR}/client/sosp_aec/base_start.sh Shenango
sudo ${AE_DIR}/Persephone/submodules/shenango/iokerneld ias 0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 noht
```
On a second server terminal
```bash
numactl -N0 -m0 ${AE_DIR}/Persephone/submodules/shenango/apps/psp_fakework/psp_fakework ${AE_DIR}/Persephone/sosp_aec/configs/base_shenango_conf 6789
```

On a client machine
```bash
sudo numactl -N0 -m0 ${AE_DIR}/client/build/src/c++/apps//client/client --config-path ${AE_DIR}/client/sosp_aec/configs/base_client_psp_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
```

You should have a similar ouput than for Perséphone if this test worked correctly.

Reproducing experiments
=======================
Throughout this section, we will be using [Shremote](Shremote) to orchestrate experiments and gather results. We will also use the provided [notebook](sosp_aec/sosp_21.pynb) to plot the figures. For each figure, we will generate the data and plot them using the corresponding notebook cell.

Setting up an orchestrator node
----------------
On a machine that is *not* one of the 7 cloudlab nodes, we will set up an environment to orchestrate experiments, gather and plot data. We provide a docker image to do so.

[Install docker](https://docs.docker.com/engine/install/ubuntu/)  (you might have to restart the docker service before running a container)

Build and start the container:
```bash
git clone --recurse-submodules https://github.com/maxdml/psp.git
cd psp
sudo docker build -t ubuntu-aec .
sudo docker run -p 8888:8888 ubuntu-aec
```

Then:
- Log in the container to configure it `bash sudo docker exec -it CONTAINER_ID /bin/bash`
- You can find the docker container ID with `sudo docker ps`
- Setup your cloudlab private key as `/root/.ssh/aec` set it to 600
- In /psp/Shremote_cfgs/config, update:
    - ssh_config.yml: set cloudlab username
    - hosts.yml: update "addr" for each machine (e.g., clnode42)
- In /psp/Shremote_cfgs/shinjuku.yml, update the server MAC address ("server_net.mac" field)

Send a dummy ssh command to each of the node to validate their certificate.
```bash
# Update USERNAME below
NODES=('236' '237' '229' '223' '240' '227' '244')
for node in ${NODES[@]}; do ssh -i /root/.ssh/aec [USERNAME]@clnode${node}.clemson.cloudlab.us 'ls /home/'; done
```

Reproducing results
----------------

Go to localhost:8888 on your browser. Open the notebook "aec.ipynb".
Each cell contains instructions to generate the data. To run the commands provided in the notebook, execute `sudo docker exec -it IMAGE_ID /bin/bash` to open a terminal in the container.
