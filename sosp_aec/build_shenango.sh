#!/bin/bash

set -e
set -x


kernver() {
  ver=$(uname -r)
  ver=${ver/-*/}
  ver=( ${ver//./ } )

  if [[ ${ver[0]} -lt 4 || ${ver[1]} -lt 15 ]]; then
    echo "Need to build with a newer kernel!"
    exit 0
  fi
}

kernver


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


