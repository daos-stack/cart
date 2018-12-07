#!/bin/bash
# Phyl -- with my changes
# The original is in DAOS_INFO/CART/brians-multi-node-test.sh

set -ex -o pipefail

# shellcheck disable=SC1091
if [ -f .localenv ]; then
    # read (i.e. environment, etc.) overrides
    . .localenv
fi

HOSTPREFIX=${HOSTPREFIX-${HOSTNAME%%.*}}
NFS_SERVER=${NFS_SERVER:-$HOSTPREFIX}

trap 'echo "encountered an unchecked return code, exiting with error"' ERR

# shellcheck disable=SC1091
. .build_vars-Linux.sh

echo "HOSTNAME=${HOSTNAME}"

if [ "$1" = "2" ]; then
    test_runner_vm=$((${EXECUTOR_NUMBER:-0}*3+7))
    vm1="$((test_runner_vm+1))"
    vm2="$((test_runner_vm+2))"
    vmrange="$vm1-$vm2"
    test_runner_vm="vm$test_runner_vm"
    vm1="vm$vm1"
    vm2="vm$vm2"
elif [ "$1" = "5" ]; then
    test_runner_vm="vm1"
    vmrange="2-6"
fi

# Phyl

echo $vm1
echo $vm2

# Phyl

log_base_path="testLogs-${1}_node"

rm -f results_1.yml IOF_[25]-node_junit.xml

trap 'set +e
i=5
# due to flakiness on wolf-53, try this several times
while [ $i -gt 0 ]; do
    pdsh -R ssh -S -w "${HOSTPREFIX}$test_runner_vm,${HOSTPREFIX}vm[$vmrange]" "set -x
    x=0
    while [ \$x -lt 30 ] &&
          grep $DAOS_BASE /proc/mounts &&
          ! sudo umount $DAOS_BASE; do
        ps axf
        sleep 1
        let x+=1
    done
    sudo sed -i -e \"/added by multi-node-test-$1.sh/d\" /etc/fstab
    sudo rmdir $DAOS_BASE || find $DAOS_BASE || true" 2>&1 | dshbak -c
    if [ ${PIPESTATUS[0]} = 0 ]; then
        i=0
    fi
    let i-=1
done' EXIT

DAOS_BASE=${SL_OMPI_PREFIX%/install/*}
if ! pdsh -R ssh -S -w "${HOSTPREFIX}$test_runner_vm,${HOSTPREFIX}vm[$vmrange]" "set -ex
ulimit -c unlimited
sudo mkdir -p $DAOS_BASE
sudo ed <<EOF /etc/fstab
\\\$a
$NFS_SERVER:$PWD $DAOS_BASE nfs defaults 0 0 # added by multi-node-test-$1.sh
.
wq
EOF
sudo mount $DAOS_BASE

# TODO: package this in to an RPM
pip3 install --user tabulate

df -h" 2>&1 | dshbak -c; then
    echo "Cluster setup (i.e. provisioning) failed"
    exit 1
fi

#echo "hit enter to continue"
#read -r
#exit 0

# shellcheck disable=SC2029
if ! ssh "${HOSTPREFIX}$test_runner_vm" "set -ex
ulimit -c unlimited
cd $DAOS_BASE

# now run it!
pushd install/Linux/TESTING/
if [ \"$1\" = \"2\" ]; then
    cat <<EOF > scripts/config.json
{
    \"should_be_host_list\": [\"${HOSTPREFIX}${vm1}\", \"${HOSTPREFIX}${vm2}\"],
    \"host_list\": [\"${HOSTPREFIX}${test_runner_vm}\", \"${HOSTPREFIX}${vm1}\"],
    \"use_daemon\": \"DvmRunner\",
    \"log_base_path\": \"$log_base_path\"
}
EOF

    rm -rf $log_base_path/
    python3 test_runner config=scripts/config.json \\
        \$(ls scripts/*.yml) || {
        rc=\${PIPESTATUS[0]}
        echo \"Test exited with \$rc\"
    }
    find $log_base_path/testRun -name subtest_results.yml \\
         -exec grep -Hi fail {} \\;
elif [ \"$1\" = \"5\" ]; then
    cat <<EOF > scripts/iof_multi_five_node.cfg
{
    \"should_be_host_list\": [
        \"${HOSTPREFIX}vm2\",
        \"${HOSTPREFIX}vm3\",
        \"${HOSTPREFIX}vm4\",
        \"${HOSTPREFIX}vm5\",
        \"${HOSTPREFIX}vm6\"
    ],
    \"host_list\": [\"${HOSTPREFIX}${test_runner_vm}\",
        \"${HOSTPREFIX}vm2\",
        \"${HOSTPREFIX}vm3\",
        \"${HOSTPREFIX}vm4\",
        \"${HOSTPREFIX}vm5\"
    ],
    \"use_daemon\": \"DvmRunner\",
    \"log_base_path\": \"$log_base_path\"
}
EOF
    rm -rf $log_base_path/
    python3 test_runner config=scripts/iof_multi_five_node.cfg \\
        \$(ls scripts/*.yml) || {
        rc=\${PIPESTATUS[0]}
        echo \"Test exited with \$rc\"
    }
    find $log_base_path/testRun -name subtest_results.yml \\
         -exec grep -Hi fail {} \\;
fi
exit \$rc"; then
    rc=${PIPESTATUS[0]}
else
    rc=0
fi

scp -r "${HOSTPREFIX}$test_runner_vm:$DAOS_BASE/install/Linux/TESTING/$log_base_path" install/Linux/TESTING/
{
    cat <<EOF
TestGroup:
    submission: $(TZ=UTC date)
    test_group: CART_${1}-node
    testhost: $HOSTNAME
    user_name: jenkins
Tests:
EOF
    find install/Linux/TESTING/"$log_base_path" \
         -name subtest_results.yml -print0 | xargs -0 cat
} > results_1.yml
cat results_1.yml

PYTHONPATH=scony_python-junit/ jenkins/autotest_utils/results_to_junit.py
ls -ltar

exit "$rc"
