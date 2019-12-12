#!/bin/bash

clear
tmux clear-history

. ./utils/setup_local.sh

export PATH=./install/Linux/TESTING/tests/:$PATH
set -ex

mkdir -p $PWD/logs/live-rank
rm -rf $PWD/logs/live-rank/*
rm -rf ./core.*

# -x FI_LOG_LEVEL=debug \

(orterun --mca mtl ^psm2,ofi  --enable-recovery \
 --output-filename $PWD/logs/live-rank \
 --report-uri ~/uri.txt \
 -x CRT_TIMEOUT=5 \
-x D_LOG_FILE=$PWD/logs/test-cancel-rpc-to-live-rank.log \
-x D_LOG_MASK=DEBUG,MEM=ERR \
-x OFI_INTERFACE=ib0 \
-x CRT_PHY_ADDR_STR="ofi+sockets" \
-x FI_PSM2_NAME_SERVER=1 \
-x FI_SOCKETS_MAX_CONN_RETRY=1 \
-x CRT_CTX_SHARE_ADDR=0 \
-x CRT_CTX_NUM=2 \
 -np 2 \
 ./install/Linux/TESTING/tests/test_cancel_rpc_to_live_rank \
 --name service-group --is_service --c 2) 
