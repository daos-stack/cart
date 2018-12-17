#!/bin/sh
# Copyright (C) 2016-2018 Intel Corporation
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

# shellcheck disable=SC2039
# A list of tests to run as a single instance on Jenkins
JENKINS_TEST_LIST=(scripts/cart_echo_test.yml                   \
                   scripts/cart_echo_test_non_sep.yml           \
                   scripts/cart_test_corpc_prefwd.yml           \
                   scripts/cart_test_corpc_prefwd_non_sep.yml   \
                   scripts/cart_test_group.yml                  \
                   scripts/cart_test_group_non_sep.yml          \
                   scripts/cart_test_barrier.yml                \
                   scripts/cart_test_barrier_non_sep.yml        \
                   scripts/cart_threaded_test.yml               \
                   scripts/cart_threaded_test_non_sep.yml       \
                   scripts/cart_test_rpc_error.yml              \
                   scripts/cart_test_rpc_error_non_sep.yml      \
                   scripts/cart_test_singleton.yml              \
                   scripts/cart_test_singleton_non_sep.yml      \
                   scripts/cart_test_corpc_version.yml          \
                   scripts/cart_test_corpc_version_non_sep.yml  \
                   scripts/cart_test_cart_ctl.yml               \
                   scripts/cart_test_cart_ctl_non_sep.yml       \
                   scripts/cart_test_iv.yml                     \
                   scripts/cart_test_iv_non_sep.yml             \
                   scripts/cart_test_proto.yml                  \
                   scripts/cart_test_proto_non_sep.yml          \
                   scripts/cart_test_no_timeout.yml             \
                   scripts/cart_test_no_timeout_non_sep.yml)

# Check for symbol names in the library.
if [ -d "utils" ]; then
  utils/test_cart_lib.sh
else
  ./test_cart_lib.sh
fi

echo "$CART_TEST_MODE"
# Possible CART_TEST_MODEs are native, memcheck, all. The default is native
# which does not use valgrind.
if [ -z "$CART_TEST_MODE"  ]; then
  CART_TEST_MODE="native"
fi

if [ -n "$COMP_PREFIX"  ]; then
  TESTDIR=${COMP_PREFIX}/TESTING
else
  TESTDIR="install/Linux/TESTING"
fi
# shellcheck disable=SC2039
if [[ "$CART_TEST_MODE" =~ (native|all) ]]; then
  echo "Nothing to do yet, wish we could fail some tests"
  scons utest
  # Added || exit to placate SC2203
  cd ${TESTDIR} || exit
  if [ "$1" = "--config" ]; then
    echo "Using config file ../../../${2}"
    # @ changed to * for SC2145
    python3 test_runner --config=../../../"${2}" "${JENKINS_TEST_LIST[@]}"
  else
    # @ changed to * for SC2145
    python3 test_runner "${JENKINS_TEST_LIST[@]}"
  fi
  cd -
fi

# shellcheck disable=SC2039
if [[ "$CART_TEST_MODE" =~ (memcheck|all) ]]; then
  echo "Nothing to do yet"
  scons utest --utest-mode=memcheck
  export TR_USE_VALGRIND=memcheck
  # Added || exit to placate SC2203
  cd ${TESTDIR} || exit
  python3 test_runner "${JENKINS_TEST_LIST[@]}"

  cd -
  RESULTS="valgrind_results"
# shellcheck disable=SC2039
  if [[ ! -e ${RESULTS} ]]; then mkdir ${RESULTS}; fi

  # Recursive copy to results, including all directories and matching files,
  # but pruning empty directories from the tree.
  rsync -rm --include="*/" --include="valgrind*xml" "--exclude=*" ${TESTDIR} ${RESULTS}

fi
