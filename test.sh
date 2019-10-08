export OFI_INTERFACE=eth0
export D_LOG_MASK=ERR
export CRT_PHY_ADDR_STR="sm"
#export CRT_PHY_ADDR_STR="ofi+sockets"
set -x

export SERVER_APP='crt_launch -e install/Linux/TESTING/tests/no_pmix_launcher_server'
export SERVER_HOST='wolf-55'
export CLIENT_APP='crt_launch -c -e install/Linux/TESTING/tests/no_pmix_launcher_client'
export CLIENT_HOST='wolf-55'

orterun --mca btl self,tcp -np 1 -H $SERVER_HOST -x D_LOG_MASK -x CRT_PHY_ADDR_STR -x OFI_INTERFACE $SERVER_APP : -np 1 -H $CLIENT_HOST -x CRT_PHY_ADDR_STR -x OFI_INTERFACE -x D_LOG_MASK $CLIENT_APP
