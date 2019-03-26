#!/bin/bash

set -e
set -x

RUN_UTEST=false

# A list of tests to run as a single instance on Jenkins
JENKINS_TEST_LIST=(                                             \
                   scripts/cart_test_group_psm2.yml                  \
                   scripts/cart_test_group_non_sep_psm2.yml          \
                  )

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

TESTDIR=${COMP_PREFIX}/TESTING

if [[ "$CART_TEST_MODE" =~ (native|all) ]]; then
  echo "Nothing to do yet, wish we could fail some tests"
  if ${RUN_UTEST:-true}; then
      scons utest
  fi
  pushd ${TESTDIR}
  python3 test_runner "${JENKINS_TEST_LIST[@]}"
  popd
fi

if [[ "$CART_TEST_MODE" =~ (memcheck|all) ]]; then
  echo "Nothing to do yet"
  if ${RUN_UTEST:-true}; then
    scons utest --utest-mode=memcheck
  fi
  export TR_USE_VALGRIND=memcheck
  pushd ${TESTDIR}
  python3 test_runner "${JENKINS_TEST_LIST[@]}"

  popd
  RESULTS="valgrind_results"
  if [[ ! -e ${RESULTS} ]]; then mkdir ${RESULTS}; fi

  # Recursive copy to results, including all directories and matching files,
  # but pruning empty directories from the tree.
  rsync -rm --include="*/" --include="valgrind*xml" "--exclude=*" ${TESTDIR} ${RESULTS}

fi
