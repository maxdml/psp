#!/bin/bash -x

if [ -z ${PSP_DIR} ]; then
    echo "PSP_DIR is unset.";
    exit;
fi

# sysctl configs
sudo sysctl kernel.nmi_watchdog=0
sudo sysctl -w kernel.watchdog=0

# Setup huge pages
sudo sh -c 'echo 8192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages;'

# Have the NIC use IGB_UIO for Perséphone and the client. Shinjuku has its own i40e driver.
# Disable turbo
sudo ${PSP_DIR}/scripts/setup/turbo.sh disable
#TODO the following steps are only necessary for non bifurcated drivers.
#TODO make the PCI ID configurable
# Unbind the NIC from the kernel driver
sudo ${PSP_DIR}/submodules/dpdk/usertools/dpdk-devbind.py --force -u 18:00.1
# Load uio to bypass the kernel and use the NIC. Need the module built for the kernel in use
sudo modprobe uio
sudo insmod ${PSP_DIR}/submodules/dpdk/x86_64-native-linuxapp-gcc/build/kernel/linux/igb_uio/igb_uio.ko
sudo ${PSP_DIR}/submodules/dpdk/usertools/dpdk-devbind.py -b igb_uio 18:00.1
