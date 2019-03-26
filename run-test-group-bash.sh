#!/bin/bash

PATH=$PWD/install/Linux/:$PWD/install/Linux/bin:$PWD/install/Linux/sbin:$PATH
LD_LIBRARY_PATH=$PWD/install/Linux/lib:$PWD/install/Linux/lib64:$LD_LIBRARY_PATH
CPATH=$PWD/install/Linux/include/:$CPATH

 CONFIG_RTE_MAX_MEMSEG=1024

 OMPI_MCA_rmaps_base_oversubscribe=1
 . ./utils/setup_local.sh
echo $PATH

#(orterun -x DD_STDERR=ERR -x OFI_INTERFACE=ib0 -x CRT_PHY_ADDR_STR=ofi+psm2 --mca mtl ^psm2,ofi --enable-recovery -np 1\

(cd /home/yulujia/tmp; \
rm -rf ./uri.txt; \
rm -rf ./test-group-server.log; \
rm -rf ./test-group-client.log; \
cd -)
which orterun
find . -iname test_group

(orterun --mca mtl ^psm2,ofi  --enable-recovery \
 --report-uri /home/yulujia/tmp/uri.txt \
-x DD_STDERR=info \
-x D_LOG_FILE=$HOME/tmp/test-group-server.log \
-x D_LOG_MASK=DEBUG \
-x OFI_INTERFACE=ib0 \
-x CRT_PHY_ADDR_STR="ofi+psm2" \
-x FI_PSM2_NAME_SERVER=1 \
-x FI_SOCKETS_MAX_CONN_RETRY=1 \
-x CRT_CTX_SHARE_ADDR=0 \
-x CRT_CTX_NUM=2 \
 -np 1 \
 ./install/Linux/TESTING/tests/test_group \
--name service-group --is_service --c 2 : \
-x DD_STDERR=info \
-x D_LOG_FILE=$HOME/tmp/test-group-client.log \
-x D_LOG_MASK=DEBUG \
-x OFI_INTERFACE=ib0 \
-x CRT_PHY_ADDR_STR="ofi+psm2" \
-x FI_PSM2_NAME_SERVER=1 \
-x FI_SOCKETS_MAX_CONN_RETRY=1 \
-x CRT_CTX_SHARE_ADDR=0 \
-x CRT_CTX_NUM=2 \
 -np 1 \
 ./install/Linux/TESTING/tests/test_group \
 --name client-group --attach_to service-group -c 2) 2>&1 | tee output-test-group.txt
