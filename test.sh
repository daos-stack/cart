export OFI_INTERFACE=eth0
export D_LOG_MASK=ERR
#export CRT_PHY_ADDR_STR="sm"
export CRT_PHY_ADDR_STR="ofi+sockets"
set -x

export SERVER_APP='orterun -np 2 crt_launch -e install/Linux/TESTING/tests/no_pmix_launcher_server'
export CLIENT_APP='install/Linux/TESTING/tests/no_pmix_launcher_client'

$SERVER_APP
echo $
#orterun --mca btl self,tcp -np 1 -H $SERVER_HOST -x D_LOG_MASK -x CRT_PHY_ADDR_STR -x OFI_INTERFACE $SERVER_APP : -np 1 -H $CLIENT_HOST -x CRT_PHY_ADDR_STR -x OFI_INTERFACE -x D_LOG_MASK $CLIENT_APP
