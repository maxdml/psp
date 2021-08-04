#!/bin/bash -x

SYS_NAME=$1

# Disable turbo
sudo ${AE_DIR}/Persephone/scripts/setup/turbo.sh disable

# sysctl configs
sudo sysctl kernel.nmi_watchdog=0
sudo sysctl -w kernel.watchdog=0

# Unbind the NIC from the kernel driver
sudo ${AE_DIR}/Persephone/submodules/dpdk/usertools/dpdk-devbind.py --force -u 18:00.1

# Setup huge pages
sudo sh -c 'echo 8192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages;'

# Have the NIC use IGB_UIO for Perséphone and the client. Shinjuku has its own i40e driver.
if [[ "$SYS_NAME" == "Persephone" || "$SYS_NAME" == "client" ]]; then
    # Load uio to bypass the kernel and use the NIC. Need the module built for the kernel in use
    sudo modprobe uio
    sudo insmod ${AE_DIR}/${SYS_NAME}/submodules/dpdk/x86_64-native-linuxapp-gcc/build/kernel/linux/igb_uio/igb_uio.ko
    sudo ${AE_DIR}/Persephone/submodules/dpdk/usertools/dpdk-devbind.py -b igb_uio 18:00.1
fi

if [[ "$SYS_NAME" == "shinjuku" ]]; then
    sudo insmod ${AE_DIR}/Persephone/submodules/shinjuku/deps/dune/kern/dune.ko
    sudo insmod ${AE_DIR}/Persephone/submodules/shinjuku/deps/pcidma/pcidma.ko
fi
