#!/bin/bash

##
## Copyright (c) 2017-18, Lawrence Livermore National Security, LLC.
##
## Produced at the Lawrence Livermore National Laboratory.
##
## LLNL-CODE-738930
##
## All rights reserved.
## 
## This file is part of the RAJA Performance Suite.
##
## For details about use and distribution, please read raja-perfsuite/LICENSE.
##

rm -rf build_lc_chaos-gcc-4.9.3 2>/dev/null
mkdir build_lc_chaos-gcc-4.9.3 && cd build_lc_chaos-gcc-4.9.3
. /usr/local/tools/dotkit/init.sh && use cmake-3.4.1

RAJA_DIR=$(git rev-parse --show-toplevel)

cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -C ${RAJA_DIR}/host-configs/lc-builds/chaos/gcc_4_9_3.cmake \
  -DENABLE_OPENMP=On \
  -DCMAKE_INSTALL_PREFIX=${RAJA_DIR}/install_lc_chaos-gcc-4.9.3 \
  "$@" \
  ${RAJA_DIR}
