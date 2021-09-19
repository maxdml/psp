#!/bin/bash -x

if [ -z ${PSP_DIR} ]; then
    echo "PSP_DIR is unset.";
    exit;
fi

# Setup synthetic work library
make -C ${PSP_DIR}/submodules/fake_work libfake

# Setup RocksDB
make -j -C ${PSP_DIR}/submodules/rocksdb static_lib

# Setup Perséphone
mkdir ${PSP_DIR}/build && cd ${PSP_DIR}/build
cmake -DCMAKE_BUILD_TYPE=Release -DDPDK_MELLANOX_SUPPORT=OFF ${PSP_DIR}
make -j -C ${PSP_DIR}/build

# TODO: setup RocksDB database for the RocksDB worker
