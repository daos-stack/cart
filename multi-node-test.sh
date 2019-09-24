#!/bin/bash
# Copyright (C) 2019 Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted for any purpose (including commercial purposes)
# provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions, and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions, and the following disclaimer in the
#    documentation and/or materials provided with the distribution.
#
# 3. In addition, redistributions of modified forms of the source or binary
#    code must carry prominent notices stating that the original code was
#    changed and the date of the change.
#
#  4. All publications or advertising materials mentioning features or use of
#     this software are asked, but not required, to acknowledge that it was
#     developed by Intel Corporation and credit the contributors.
#
# 5. Neither the name of Intel Corporation, nor the name of any Contributor
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -ex -o pipefail

# shellcheck disable=SC1091
if [ -f .localenv ]; then
    # read (i.e. environment, etc.) overrides
    . .localenv
fi

NODE_COUNT="$1"

NFS_SERVER=${NFS_SERVER:-${HOSTNAME%%.*}}

trap 'echo "encountered an unchecked return code, exiting with error"' ERR

read -r -a nodes <<< "${2//,/ }"
if [ "$NODE_COUNT" = "5" ]; then
    # first node is a test runner, get rid of it
    read -r -a nodes <<< "${nodes[*]:1}"
fi
#reduce set down to desired number
read -r -a nodes <<< "${nodes[*]:0:$NODE_COUNT}"

if [ -z "$CART_TEST_MODE"  ]; then
  CART_TEST_MODE="native"
fi

if [ "$CART_TEST_MODE" == "memcheck" ]; then
  CART_DIR="${1}vgd"
else
  CART_DIR="${1}"
fi

# For nodes that are only rebooted between CI nodes left over mounts
# need to be cleaned up.
pre_clean () {
    i=5
    while [ $i -gt 0 ]; do
        if clush "${CLUSH_ARGS[@]}" -B -l "${REMOTE_ACCT:-jenkins}" -R ssh \
              -S -w "$(IFS=','; echo "${nodes[*]}")" "set -ex
              mapfile -t mounts < <(grep 'added by multi-node-test-$NODE_COUNT.sh' /etc/fstab)
              for n_mnt in \"\${mounts[@]}\"; do
                  mpnt=(\${n_mnt})
                  sudo umount \${mpnt[1]}
              done
              sudo sed -i -e \"/added by multi-node-test-$NODE_COUNT.sh/d\" /etc/fstab"; then
            break
        fi
        ((i-=1)) || true
    done
}

# shellcheck disable=SC1004
# shellcheck disable=SC2154
trap 'set +e
i=5
while [ $i -gt 0 ]; do
        if clush "${CLUSH_ARGS[@]}" -B -l "${REMOTE_ACCT:-jenkins}" -R ssh \
             -S -w "$(IFS=','; echo "${nodes[*]}")" "set -x
    x=0
    rc=0
    while [ \$x -lt 30 ] &&
          grep $CART_BASE /proc/mounts &&
          ! sudo umount $CART_BASE; do
        # CART-558 - cart tests leave orte processes around
        if pgrep -a cart_ctl; then
            echo "FAIL: cart_ctl found left running on \$HOSTNAME"
            rc=1
        fi
        pgrep -a \(orte-dvm\|orted\|cart_ctl\)
        pkill \(orte-dvm\|orted\|cart_ctl\)
        sleep 1
        let x+=1
    done
    sudo sed -i -e \"/added by multi-node-test-$1.sh/d\" /etc/fstab
    sudo rmdir $CART_BASE || find $CART_BASE || true
    if [ \$rc != 0 ]; then
        exit \$rc
    fi" 2>&1 | dshbak -c
    if [ ${PIPESTATUS[0]} = 0 ]; then
        i=0
    fi
    let i-=1
done' EXIT

pre_clean

# shellcheck disable=SC1091
. .build_vars-Linux.sh

read -r -a CLUSH_ARGS <<< "$CLUSH_ARGS"

CART_BASE="${SL_PREFIX%/install/*}"
if ! clush "${CLUSH_ARGS[@]}" -B -l "${REMOTE_ACCT:-jenkins}" -R ssh -S \
    -w "$(IFS=','; echo "${nodes[*]}")" "set -ex
ulimit -c unlimited
sudo mkdir -p $CART_BASE
sudo ed <<EOF /etc/fstab
\\\$a
$NFS_SERVER:$PWD $CART_BASE nfs defaults 0 0 # added by multi-node-test-$NODE_COUNT.sh
.
wq
EOF
sudo mount $CART_BASE

# TODO: package this in to an RPM
pip3 install --user tabulate

df -h"; then
    echo "Cluster setup (i.e. provisioning) failed"
    exit 1
fi

TEST_TAG="${3:-quick}"

TESTDIR=${SL_PREFIX}/TESTING

LOGDIR="$TESTDIR/avocado/job-results/CART_${CART_DIR}node"

# shellcheck disable=SC2029
# shellcheck disable=SC2086
if ! ssh $SSH_KEY_ARGS ${REMOTE_ACCT:-jenkins}@"${nodes[0]}" "set -ex
ulimit -c unlimited
cd $CART_BASE

mkdir -p ~/.config/avocado/
cat <<EOF > ~/.config/avocado/avocado.conf
[datadir.paths]
logs_dir = $LOGDIR

[sysinfo.collectibles]
# File with list of commands that will be executed and have their output
# collected
commands = \$HOME/.config/avocado/sysinfo/commands
EOF

mkdir -p ~/.config/avocado/sysinfo/
cat <<EOF > ~/.config/avocado/sysinfo/commands
ps axf
dmesg
df -h
EOF

    # apply patch for https://github.com/avocado-framework/avocado/pull/3076/
    if ! grep TIMEOUT_TEARDOWN \
        /usr/lib/python2.7/site-packages/avocado/core/runner.py; then
        sudo yum -y install patch
        sudo patch -p0 -d/ << \"EOF\"
From d9e5210cd6112b59f7caff98883a9748495c07dd Mon Sep 17 00:00:00 2001
From: Cleber Rosa <crosa@redhat.com>
Date: Wed, 20 Mar 2019 12:46:57 -0400
Subject: [PATCH] [RFC] Runner: add extra timeout for tests in teardown

The current time given to tests performing teardown is pretty limited.
Let's add a 60 seconds fixed timeout just for validating the idea, and
once settled, we can turn that into a configuration setting.

Signed-off-by: Cleber Rosa <crosa@redhat.com>
---
 avocado/core/runner.py             | 11 +++++++++--
 examples/tests/longteardown.py     | 29 +++++++++++++++++++++++++++++
 selftests/functional/test_basic.py | 18 ++++++++++++++++++
 3 files changed, 56 insertions(+), 2 deletions(-)
 create mode 100644 examples/tests/longteardown.py

diff --git /usr/lib/python2.7/site-packages/avocado/core/runner.py.old /usr/lib/python2.7/site-packages/avocado/core/runner.py
index 1fc84844b..17e6215d0 100644
--- /usr/lib/python2.7/site-packages/avocado/core/runner.py.old
+++ /usr/lib/python2.7/site-packages/avocado/core/runner.py
@@ -45,6 +45,8 @@
 TIMEOUT_PROCESS_DIED = 10
 #: when test reported status but the process did not finish
 TIMEOUT_PROCESS_ALIVE = 60
+#: extra timeout to give to a test in TEARDOWN phase
+TIMEOUT_TEARDOWN = 60
 
 
 def add_runner_failure(test_state, new_status, message):
@@ -219,7 +221,7 @@ def finish(self, proc, started, step, deadline, result_dispatcher):
         wait.wait_for(lambda: not proc.is_alive() or self.status, 1, 0, step)
         if self.status:     # status exists, wait for process to finish
             deadline = min(deadline, time.time() + TIMEOUT_PROCESS_ALIVE)
-            while time.time() < deadline:
+            while time.time() < deadline + TIMEOUT_TEARDOWN:
                 result_dispatcher.map_method('test_progress', False)
                 if wait.wait_for(lambda: not proc.is_alive(), 1, 0, step):
                     return self._add_status_failures(self.status)
@@ -422,7 +424,12 @@ def sigtstp_handler(signum, frame):     # pylint: disable=W0613
 
         while True:
             try:
-                if time.time() >= deadline:
+                now = time.time()
+                if test_status.status.get('phase') == 'TEARDOWN':
+                    reached = now >= deadline + TIMEOUT_TEARDOWN
+                else:
+                    reached = now >= deadline
+                if reached:
                     abort_reason = \"Timeout reached\"
                     try:
                         os.kill(proc.pid, signal.SIGTERM)
EOF
    fi
# apply fix for https://github.com/avocado-framework/avocado/issues/2908
sudo ed <<EOF /usr/lib/python2.7/site-packages/avocado/core/runner.py
/TIMEOUT_TEST_INTERRUPTED/s/[0-9]*$/60/
wq
EOF
# apply fix for https://github.com/avocado-framework/avocado/pull/2922
if grep \"testsuite.setAttribute('name', 'avocado')\" \
    /usr/lib/python2.7/site-packages/avocado/plugins/xunit.py; then
    sudo ed <<EOF /usr/lib/python2.7/site-packages/avocado/plugins/xunit.py
/testsuite.setAttribute('name', 'avocado')/s/'avocado'/\
os.path.basename(os.path.dirname(result.logfile))/
wq
EOF
fi

# put yaml files back
restore_dist_files() {
    local dist_files=\"\$*\"

    for file in \$dist_files; do
        if [ -f \"\$file\".dist ]; then
            mv -f \"\$file\".dist \"\$file\"
        fi
    done

}

# set our machine names
mapfile -t yaml_files < <(find \"$TESTDIR\" -name \\*.yaml)

trap 'set +e; restore_dist_files \"\${yaml_files[@]}\"' EXIT

# shellcheck disable=SC2086
sed -i.dist -e \"s/- boro-A/- ${nodes[0]}/g\" \
            -e \"s/- boro-B/- ${nodes[1]}/g\" \
            -e \"s/- boro-C/- ${nodes[2]}/g\" \
            -e \"s/- boro-D/- ${nodes[3]}/g\" \
            -e \"s/- boro-E/- ${nodes[4]}/g\" \
            -e \"s/- boro-F/- ${nodes[5]}/g\" \
            -e \"s/- boro-G/- ${nodes[6]}/g\" \
            -e \"s/- boro-H/- ${nodes[7]}/g\" \"\${yaml_files[@]}\"

# let's output to a dir in the tree
rm -rf \"$TESTDIR/avocado\" \"./*_results.xml\"
mkdir -p \"$LOGDIR\"

# shellcheck disable=SC2154
trap 'set +e restore_dist_files \"\${yaml_files[@]}\"' EXIT

pushd \"$TESTDIR\"

# now run it!
export PYTHONPATH=./util
export CART_TEST_MODE=\"$CART_TEST_MODE\"
if ! ./launch.py -s \"$TEST_TAG\"; then
    rc=\${PIPESTATUS[0]}
else
    rc=0
fi

mkdir -p \"testLogs-${CART_DIR}_node\"

cp -r testLogs/* \"testLogs-${CART_DIR}_node\"

exit \$rc"; then
    rc=${PIPESTATUS[0]}
else
    rc=0
fi

mkdir -p install/Linux/TESTING/avocado/job-results

# shellcheck disable=SC2086
scp $SSH_KEY_ARGS -r "${REMOTE_ACCT:-jenkins}"@"${nodes[0]}":\
"$TESTDIR"/testLogs-"${CART_DIR}"_node install/Linux/TESTING/

# shellcheck disable=SC2086
scp $SSH_KEY_ARGS -r "${REMOTE_ACCT:-jenkins}"@"${nodes[0]}":"$LOGDIR" \
                                      install/Linux/TESTING/avocado/job-results

exit "$rc"
