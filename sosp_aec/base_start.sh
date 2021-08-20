#!/bin/bash -x

SYS_NAME=$1

# sysctl configs
sudo sysctl kernel.nmi_watchdog=0
sudo sysctl -w kernel.watchdog=0

# Setup huge pages
sudo sh -c 'echo 8192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages;'

kernver() {
  ver=$(uname -r)
  ver=${ver/-*/}
  ver=( ${ver//./ } )

  if [[ ${ver[0]} -lt $1 || ${ver[1]} -lt $2 ]]; then
    echo "Need to use a newer kernel!"
    exit 0
  fi
}

# Have the NIC use IGB_UIO for Perséphone and the client. Shinjuku has its own i40e driver.
if [[ "$SYS_NAME" == "Persephone" || "$SYS_NAME" == "client" ]]; then
    # Disable turbo
    sudo ${AE_DIR}/${SYS_NAME}/scripts/setup/turbo.sh disable
    # Unbind the NIC from the kernel driver
    sudo ${AE_DIR}/${SYS_NAME}/submodules/dpdk/usertools/dpdk-devbind.py --force -u 18:00.1
    # Load uio to bypass the kernel and use the NIC. Need the module built for the kernel in use
    sudo modprobe uio
    sudo insmod ${AE_DIR}/${SYS_NAME}/submodules/dpdk/x86_64-native-linuxapp-gcc/build/kernel/linux/igb_uio/igb_uio.ko
    sudo ${AE_DIR}/${SYS_NAME}/submodules/dpdk/usertools/dpdk-devbind.py -b igb_uio 18:00.1
fi

if [[ "$SYS_NAME" == "Shenango" ]]; then

    kernver 4 15

    # Disable turbo
    sudo ${AE_DIR}/Persephone/scripts/setup/turbo.sh disable
    # Unbind the NIC from the kernel driver

    # Unbind the NIC from the kernel driver
    sudo ${AE_DIR}/Persephone/submodules/shenango/dpdk/usertools/dpdk-devbind.py -u --force 18:00.1
    # Load uio to bypass the kernel and use the NIC. Need the module built for the kernel in use
    sudo modprobe uio
    sudo insmod ${AE_DIR}/Persephone/submodules/shenango/dpdk/build/kmod/igb_uio.ko
    sudo ${AE_DIR}/Persephone/submodules/shenango/dpdk/usertools/dpdk-devbind.py -b igb_uio 18:00.1

    # needed for the iokernel's shared memory
    sudo sysctl -w kernel.shm_rmid_forced=1
    sudo sysctl -w kernel.shmmax=18446744073692774399
    sudo sysctl -w vm.hugetlb_shm_group=27
    sudo sysctl -w vm.max_map_count=16777216
    sudo sysctl -w net.core.somaxconn=3072

    # set up the ksched module
    sudo rmmod ksched
    sudo rm /dev/ksched
    sudo insmod ${AE_DIR}/Persephone/submodules/shenango/ksched/build/ksched.ko
    sudo mknod /dev/ksched c 280 0
    sudo chmod uga+rwx /dev/ksched
fi

if [[ "$SYS_NAME" == "shinjuku" ]]; then
    # Disable turbo
    sudo ${AE_DIR}/Persephone/scripts/setup/turbo.sh disable
    # Unbind the NIC from the kernel driver
    sudo ${AE_DIR}/Persephone/submodules/shinjuku/deps/dpdk/tools/dpdk_nic_bind.py --force -u 18:00.1
    sudo insmod ${AE_DIR}/Persephone/submodules/shinjuku/deps/dune/kern/dune.ko
    sudo insmod ${AE_DIR}/Persephone/submodules/shinjuku/deps/pcidma/pcidma.ko
fi

sudo mkdir -p /tmpfs
mountpoint -q /tmpfs || sudo mount -t tmpfs -o size=50G,mode=1777 tmpfs /tmpfs
mkdir -p /tmpfs/experiments/
