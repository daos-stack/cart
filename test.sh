set -x

#export CRT_PHY_ADDR_STR="ofi+verbs;ofi_rxm"
#export OFI_INTERFACE=mlx4_0

export CRT_PHY_ADDR_STR="ofi+sockets"
export OFI_INTERFACE=eth0

ORTE_EXPORTS="-x CRT_PHY_ADDR_STR -x OFI_INTERFACE "
ORTE_FLAGS="--mca pml ob1 "

orterun ${ORTE_FLAGS} -np 12 ${ORTE_EXPORTS} ./install/Linux/bin/crt_launch -e ./install/Linux/TESTING/tests/generic_server
