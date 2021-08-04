Perséphone
==========

This repository hosts our artifact for the SOSP'21 Artifact Evaluation Committee.
Perséphone is a kernel-bypass request scheduler. It is designed to improve tail latency for short microsecond scale requests in heavy-tailed workloads. We evaluate Perséphone against two competing systems, [Shenango](https://www.usenix.org/conference/nsdi19/presentation/ousterhout) and [Shinjuku](https://www.usenix.org/conference/nsdi19/presentation/kaffes).

We will use cloudlab to reproduce the paper's results. Because Shinjuku requires a specific version of Linux 4.4.0, we break down the evaluation in two steps: gathering and plotting results on Shinjuku and Perséphone first, then gathering data for Shenango and updating the plots.

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

Simple client-server tests
---------------------------------
We will now make sure that all three systems work (Perséphone, Shinjuku, Shenango).

In the following steps, please set the "log_dir" entry to "${AE_DIR}/experiments/" in the YAML configuration files.

### Perséphone
On the server:
```bash
# base_start.sh only needed if calling psp-app for the first time
${AE_DIR}/Persephone/sosp_aec/base_start.sh Persephone
sudo numactl -N0 -m0 ${AE_DIR}/Persephone/build/src/c++/apps/app/psp-app --cfg ${AE_DIR}/Persephone/sosp_aec/configs/base_psp_cfg.yml --label test
```

On one client:
```bash
${AE_DIR}/Persephone/sosp_aec/base_start.sh client
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

### Shenango

Reproducing experiments
=======================
Throughout this section, we will be using [Shremote](Shremote) to orchestrate experiments and gather results. We will also use the provided [notebook](sosp_aec/sosp_21.pynb) to plot the figures. For each figure, we will generate the data and plot them using the corresponding notebook cell.

Setting up an orchestrator node
----------------
On a machine that is *not* one of the 7 cloudlab nodes, we will set up Shremote and a jupyter notebook. You will use this machine to orchestrate experiments, gather and plot data.

```bash
sudo apt install python3-pip python3-venv
python3 -m pip install --user virtualenv
export AE_DIR=/proj/psp-PG0/maxdml/Persephone
git clone --recurse-submodules git@github.com:maxdml/Persephone.git ${AE_DIR}
git submodule update Shremote
cd ${AE_DIR}
git checkout sosp_aec
```

We now need to update Shremote configurations. In ${AE_DIR}/Shremote_cfgs/config, update:
- ssh_config.yml: fill your credential on the cloudlab machines
- hosts.yml: update the "addr" for each machine
- dirs.yml: update the paths for your NFS directory in cloudlab. "shremote_cfgs" for the orchestrator node; "log_dir", "psp_apps" and "clt" for the server and clinet machines, respectively

Send a dummy ssh command from the orchestrator to each of the node to validate their certificate. For example:
```bash
# ssh -i ~/path/to/your/private/key user@clnode204.clemson.cloudlab.us 'ls /home/'
ssh -i ~/.ssh/cl2 clnode204.clemson.cloudlab.us 'ls /home/'
```

Let's set up a virtual environment
```bash
python3 -m venv env
source env/bin/activate
```

Let's test that Shremote works. On the server node
```bash
${AE_DIR}Persephone/sosp_aec/base_start.sh Persephone
```

On the orchestrator node:
```bash
```

Figure 3
--------
This command will run three experiments, one per scheduling policy.

```bash
${AE_DIR/Persephone/Shremote_cfgs/run.py 0 psp DISP2 --policies DARC CFCFS DFCFS
```

Then, run cell #x in the notebook.

Figure 4
--------

**TODO**
- This requires manual configuration of the cores given to DARC
- Make it configurable in the server: e.g. "manual" flag with number of cores for the short requests

Figure 5-a
--------
```bash
./run.py 0 psp DISP2
./run.py 0 shinjuku DISP2
./run.py 0 shenango DISP2
```

Figure 5-b
--------
```bash
./run.py 0 psp SBIM2
./run.py 0 shinjuku SBIM2
./run.py 0 shenango SBIM2
```

Figure 6
--------

Figure 7
--------

Figure 8
--------


TODOS
---------------------------------
I. Setup
- Check and add Shenango

II. Experiments
1) Setup shremote and try with the base config
- Add schedule for the same base test.
- Test with all systems

2) Notebook & figure 3
- Add to this branch the python code to parse data

3) Figure 4
- Update psp to receive configuration for figure 4

4) Figure 5
- Shinjuku build and run needs path to config file in shremote config programs.yml
- Preemption tick for shinjuku

5) Figure 6
6) Figure 7
7) Figure 8

III. Public repo
- psp-light and clt-light branches
- RocksDB submodule, Shinjuku submodule
- Remove all obsolete files
- Remove all MSR licenses
- Clean up shremote cfg dir and schedules
- remove my info from ssh_config.yml

ENHANCEMENTS
--------------
- sed /etc/default/grub rather than manual update
- Generate all yml file for log_dir, remote_mac (sjk client), etc
- Automate choice of "default" for apt update
    * apt Dpkg::Options::="--force-confold" upgrade?
- Provide a cloudlab script for any machine to setup the orchestrator
- Change schedule names to match figure numbers?
- find better names (e.g. for env variables)
- Note some "good" cloudlab nodes (and maybe bad ones too)
- Make sure we remind ppl to
     * select the right kernel when rebooting (insn do 1 time pick)
     * export AE_DIR at startup


DOCKER
----
- [Install docker](https://docs.docker.com/engine/install/ubuntu/)  (you might have to restart the docker service before running a container)
- docker pull ubuntu:18.04
