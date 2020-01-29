killall forward_server

APP="./install/Linux/TESTING/tests/forward_server"
#LAUNCH="valgrind ./install/Linux/bin/crt_launch"
LAUNCH="./install/Linux/bin/crt_launch"

#export CRT_PHY_ADDR_STR="ofi+sockets"
#export OFI_INTERFACE="eth0"

export CRT_PHY_ADDR_STR="ofi+psm2"
export OFI_INTERFACE="ib0"
export OFI_DOMAIN="hfi1_0"
export CRT_CTX_NUM=8
export CRT_CTX_SHARE_ADDR=1
export PSM2_MULTI_EP=1

orterun --mca pml ob1 --mca btl tcp,self --mca oob tcp --mca mtl ^psm2,ofi  -np 3 -H wolf-118,wolf-119,wolf-120 -x FI_PSM2_DISCONNECT=1 -x PSM2_MULTI_EP -x CRT_CTX_NUM -x CRT_CTX_SHARE_ADDR -x CRT_PHY_ADDR_STR -x OFI_INTERFACE -x OFI_DOMAIN ${LAUNCH} -e ${APP}
