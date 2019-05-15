#!/bin/bash
# Copyright (C) 2016-2019 Intel Corporation
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

set -e
set -x

# Check for symbol names in the library.
if [ -d "utils" ]; then
  utils/test_cart_lib.sh
  build_vars="./.build_vars-Linux.sh"
else
  ./test_cart_lib.sh
  build_vars="../.build_vars-Linux.sh"
fi
# Run the tests from the install TESTING directory
if [ -z "$CART_TEST_MODE"  ]; then
  CART_TEST_MODE="native"
fi

if [ -z "$COMP_PREFIX"  ]; then
  COMP_PREFIX="install/Linux"
  if [ -f "${build_vars}" ]; then
    source "${build_vars}"
    COMP_PREFIX="$SL_PREFIX"
  fi
fi

TEST_TAG="${1:-quick}"

IFS=" " read -r -a nodes <<< "${2//,/ }"

# put yaml files back
restore_dist_files() {
    local dist_files="$*"

    for file in $dist_files; do
        if [ -f "$file".dist ]; then
            mv -f "$file".dist "$file"
        fi
    done

}

TESTDIR=${COMP_PREFIX}/TESTING

# set our machine names
mapfile -t yaml_files < <(find "$TESTDIR" -name "\*.yaml")

trap 'set +e; restore_dist_files "${yaml_files[@]}"' EXIT

# shellcheck disable=SC2086
sed -i.dist -e "s/- boro-A/- ${nodes[0]}/g" \
            -e "s/- boro-B/- ${nodes[1]}/g" \
            -e "s/- boro-C/- ${nodes[2]}/g" \
            -e "s/- boro-D/- ${nodes[3]}/g" \
            -e "s/- boro-E/- ${nodes[4]}/g" \
            -e "s/- boro-F/- ${nodes[5]}/g" \
            -e "s/- boro-G/- ${nodes[6]}/g" \
            -e "s/- boro-H/- ${nodes[7]}/g" "${yaml_files[@]}"

# let's output to a dir in the tree
rm -rf "$TESTDIR/avocado" "./*_results.xml"
mkdir -p "$TESTDIR/avocado/job-results"

# remove test_runner dir until scons_local is updated
rm -rf "$TESTDIR/test_runner"

# shellcheck disable=SC2154
trap 'set +e restore_dist_files "${yaml_files[@]}"' EXIT

if [[ "$CART_TEST_MODE" =~ (native|all) ]]; then
  echo "Nothing to do yet, wish we could fail some tests"
  if ${RUN_UTEST:-true}; then
      scons utest
  fi
  #pushd ${TESTDIR}
  #python3 test_runner "${JENKINS_TEST_LIST[@]}"
  #popd
fi

if [[ "$CART_TEST_MODE" =~ (memcheck|all) ]]; then
  echo "Nothing to do yet"
  if ${RUN_UTEST:-true}; then
    scons utest --utest-mode=memcheck
  fi
  #export TR_USE_VALGRIND=memcheck
  #pushd ${TESTDIR}
  #python3 test_runner "${JENKINS_TEST_LIST[@]}"
  #popd
  RESULTS="valgrind_results"
  if [[ ! -e ${RESULTS} ]]; then mkdir ${RESULTS}; fi

  # Recursive copy to results, including all directories and matching files,
  # but pruning empty directories from the tree.
  rsync -rm --include="*/" --include="valgrind*xml" "--exclude=*" ${TESTDIR} ${RESULTS}

fi

CART_BASE="\${SL_PREFIX%/install*}"

# shellcheck disable=SC2029
if ! ssh -i ci_key jenkins@"${nodes[0]}" "set -ex
ulimit -c unlimited
cd $CART_BASE

mkdir -p ~/.config/avocado/
cat <<EOF > ~/.config/avocado/avocado.conf
[datadir.paths]
logs_dir = $TESTDIR/avocado/job-results

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

# apply fix for https://github.com/avocado-framework/avocado/issues/2908
sudo ed <<EOF /usr/lib/python2.7/site-packages/avocado/core/runner.py
/TIMEOUT_TEST_INTERRUPTED/s/[0-9]*$/60/
wq
EOF
# apply fix for https://github.com/avocado-framework/avocado/pull/2922
if grep \"testsuite.setAttribute('name', 'avocado')\" \
    /usr/lib/python2.7/site-packages/avocado/plugins/xunit.py; then
    sudo ed <<EOF /usr/lib/python2.7/site-packages/avocado/plugins/xunit.py
/testsuite.setAttribute('name', 'avocado')/s/'avocado'/os.path.basename(os.path.dirname(result.logfile))/
wq
EOF
fi
pushd $TESTDIR

# now run it!
export PYTHONPATH=./util
if ! ./launch.py -s \"$TEST_TAG\"; then
    rc=\${PIPESTATUS[0]}
else
    rc=0
fi

exit \$rc"; then
    rc=${PIPESTATUS[0]}
else
    rc=0
fi

exit "$rc"

