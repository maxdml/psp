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

On the client machines:
```bash
export AE_DIR=/usr/local/sosp
git clone --recurse-submodules https://github.com/maxdml/psp.git ${AE_DIR}/client; cd ${AE_DIR}/client; git checkout client; mkdir ${AE_DIR}/client/build; cd ${AE_DIR}/client/build; cmake -DCMAKE_BUILD_TYPE=Release -DDPDK_MELLANOX_SUPPORT=OFF ${AE_DIR}/client; make -j -C ${AE_DIR}/client/build
```

Simple client-server tests
---------------------------------
We will now make sure that all three systems work (Perséphone, Shinjuku, Shenango).

In the following steps, please set the "log_dir" entry to "${AE_DIR}/experiments/" in the YAML configuration files.

### Perséphone
On the server:
```bash
${AE_DIR}/Persephone/sosp_aec/base_start.sh Persephone
cd ${AE_DIR}/Persephone/build/src/c++/apps/app/
sudo numactl -N0 -m0 psp-app --cfg ${AE_DIR}/Persephone/sosp_aec/configs/base_psp_cfg.yml --label test
```

On one client:
```bash
${AE_DIR}/client/sosp_aec/base_start.sh client
sudo numactl -N0 -m0 ${AE_DIR}/client/build/src/c++/apps//client/client --config-path ${AE_DIR}/client/sosp_aec/configs/base_client_psp_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
```

If you see an output similar to the one bellow, the system works as expected. You can ignore the Fdir error.
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
On the server
```bash
# We use base_start.sh to unbind the NIC from igb_uio
${AE_DIR}/Persephone/sosp_aec/base_start.sh shinjuku
sudo numactl -N0 -m0 ${AE_DIR}/Persephone/submodules/shinjuku/build_and_run.sh ${AE_DIR}/Persephone/sosp_aec/configs/base_shinjuku_conf
```

On the client update the server's NIC MAC address in the config file.
One way to find the NIC MAC ID is through the Cloudlab portal, by clicking on the node. It should be the only 10Gbps NIC, eth1.
In ${AE_DIR}/Persephone/sosp_aec/configs/base_client_sjk_cfg.yml, put that value in the field "remote_mac".
```bash
sudo numactl -N0 -m0 ${AE_DIR}/client/build/src/c++/apps//client/client --config-path ${AE_DIR}/client/sosp_aec/configs/base_client_sjk_cfg.yml --label test --ip 192.168.10.10 --port 6789 --max-concurrency -1 --sample -1 --collect-logs 1 --outdir client0
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
- Log in the container to configure it `bash sudo docker exec -it IMAGE /bin/bash`
- Setup your cloudlab private key in the container and set it to 600
- In /psp/Shremote_cfgs/config, update:
    - ssh_config.yml: ssh credential to cloudlab
    - hosts.yml: update "addr" for each machine (e.g., clnode42)
    - dirs.yml: set "log_dir" to a desirable path on cloudlab for storing results

Send a dummy ssh command to each of the node to validate their certificate.
```bash
NODES=('236' '237' '229' '223' '240' '227' '244')
for node in ${NODES[@]}; do ssh -i ~/path/to/your/private/key user@clnode${node}.clemson.cloudlab.us 'ls /home/'; done
```

Reproducing results
----------------

Then go to localhost:8888 on your browser. Open the notebook "aec.ipynb".
The notebook has one cell per figure in the paper. You can gather data from the notebook or the terminal. Each cell has a commented command to run a script. However, this is a somewhat lengthy process and you might want to run them in a terminal. To do so, execute `sudo docker exec -it IMAGE_ID /bin/bash`, then run the command provided in the notebook.


TODOS
---------------------------------
I. Setup
- Check and add Shenango

II. Experiments

- find a way to call base_start when switching systems
- Call out which figure is long to plot and why

3) Figure 4
- Figure 4: update CFCFS line

4) Figure 5
- Shinjuku build and run needs path to config file in shremote config programs.yml
- Preemption tick for shinjuku
- Figure out the drops & stuff for shinjuku

5) Figure 6
6) Figure 7
7) Figure 8

III. Public repo
- Cleanup all unneeded files
- Go over each file. Check leak of personal info (e.g. ssh_config.yml)

ENHANCEMENTS
--------------
- Generate base shinjuku config remote_mac
- Automate choice of "default" for apt update (curse UI still shows)
- Note some "good" cloudlab nodes (and maybe bad ones too)
- Make sure we remind ppl to
     * select the right kernel when rebooting (insn do 1 time pick?)
     * export AE_DIR at startup
     * base_start and stuff when rebooting
- Figure 4: add a circle at the point (instead of the current line)
