#!/bin/bash

. ./utils/setup_local.sh

cd install/Linux/TESTING

rm -rf testLogs/*; rm -rf /tmp/core-no_pmix_group_t*; rm -rf /tmp/crt_launch-info-*;

orterun --mca mtl ^psm2,ofi  \
 -x D_LOG_FILE=testLogs/output.log -x D_LOG_MASK=DEBUG,MEM=ERR -x CRT_PHY_ADDR_STR=ofi+psm2 -x OFI_INTERFACE=ib0  -x FI_PSM2_NAME_SERVER=1  -x CRT_CTX_NUM=8 -np 8 ../bin/crt_launch -e tests/no_pmix_group_test 

cd -
