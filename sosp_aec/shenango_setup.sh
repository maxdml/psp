#!/bin/bash

set -e
set -x
pushd ${AE_DIR}/Persephone/submodules/shenango
make submodules
make -j

pushd ksched
make -j
popd

pushd bindings/cc
make -j
popd

pushd apps/psp_fakework
make -j
popd

pushd apps/rocksdb
./rocks.sh
make -j
popd

sudo ./scripts/setup_machine.sh
