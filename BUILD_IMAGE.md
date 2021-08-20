This document gives more details about steps to configure the machines.

**PROTIP**: use a terminal such as [terminator](https://terminator-gtk3.readthedocs.io/en/latest/) to broadcast commands across multiple tabs.

Steps have to be done on all machines, otherwise specified.

### Base setup
```bash
# Install required packages.
sudo apt-get update && sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" upgrade -y; sudo apt install -y cmake libaio-dev libcunit1-dev libjemalloc-dev libmnl-dev libnl-3-dev libnl-route-3-dev libboost-program-options-dev libboost-system-dev libboost-chrono-dev libboost-context-dev libnuma-dev libyaml-cpp-dev liblz4-dev libgflags-dev libsnappy-dev numactl msr-tools htop libconfig-dev software-properties-common; sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test; sudo apt update; sudo apt install -y gcc-7 g++-7; sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-7
export AE_DIR=/usr/local/sosp
```
Take the first NUMA node out of CFS' domain and disable kaslr
In _/etc/default/grub_, append the following line to the entry "GRUB_CMDLINE_LINUX_DEFAULT"
> nokaslr isolcpus=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 nohz=on nohz_full=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 maxcpus=64

Clone the repo on the server machine:
```bash
git clone --recurse-submodules https://github.com/maxdml/psp.git ${AE_DIR}/Persephone
```

Shinjuku requires Linux 4.4.0-187 and Shenango Linux 4.15, so we will install them on the *server* machine:
```bash
sudo apt install -y linux-headers-4.4.0-187-generic linux-modules-4.4.0-187-generic linux-image-4.4.0-187-generic linux-headers-4.15.0-142-generic linux-modules-4.15.0-142-generic linux-image-4.15.0-142-generic
# Next boot will be on 4.4.0-187
sudo ${AE_DIR}/Persephone/scripts/setup/pick_kernel.sh 4.4.0-187-generic
```

Finally reboot all the nodes:
```bash
# Update grub to apply these changes
sudo update-grub; sudo reboot
```

Check whether the change was correctly applied:
```bash
# On each machine (kernel image may vary)
cat /proc/cmdline
BOOT_IMAGE=/boot/vmlinuz-4.4.0-210-generic root=UUID=ce184cb1-3771-4a20-b6cd-8e9a4649a561 ro console=ttyS0,115200 nokaslr isolcpus=0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62 nohz=on nohz_full=0,2,4,6,8,10,$
# On the server machine
$ uname -r
4.4.0-187-generic
# export AE_DIR again
export AE_DIR=/usr/local/sosp
```

Setup Perséphone and Shinjuku on the server machine:
```bash
${AE_DIR}/Persephone/sosp_aec/base_setup.sh
```

Reboot on 4.15 to build Shenango:
```bash
sudo ${AE_DIR}/Persephone/scripts/setup/pick_kernel.sh 4.15.0-142-generic
# Next boot will be on 4.15.0-142
sudo reboot
```
Then:
```bash
export AE_DIR=/usr/local/sosp
${AE_DIR}/Persephone/sosp_aec/build_shenango.sh
```

On the client machines:
```bash
git clone --recurse-submodules https://github.com/maxdml/psp.git ${AE_DIR}/client
cd ${AE_DIR}/client
git checkout client
mkdir ${AE_DIR}/client/build
cd ${AE_DIR}/client/build
cmake -DCMAKE_BUILD_TYPE=Release -DDPDK_MELLANOX_SUPPORT=OFF ${AE_DIR}/client
make -j -C ${AE_DIR}/client/build
```
