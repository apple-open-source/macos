#!/bin/sh

# shell script goes here
set -e

BZIP2=${BUILT_PRODUCTS_DIR}/bzip2
BZIP2_SOURCE=${SRCROOT}/bzip2

printf "[TEST] Standard bzip2 Tests\n"

export DYLD_LIBRARY_PATH=${BUILT_PRODUCTS_DIR}
${BZIP2} -1  < ${BZIP2_SOURCE}/sample1.ref > ${BUILT_PRODUCTS_DIR}/sample1.rb2
${BZIP2} -2  < ${BZIP2_SOURCE}/sample2.ref > ${BUILT_PRODUCTS_DIR}/sample2.rb2
${BZIP2} -3  < ${BZIP2_SOURCE}/sample3.ref > ${BUILT_PRODUCTS_DIR}/sample3.rb2
${BZIP2} -d  < ${BZIP2_SOURCE}/sample1.bz2 > ${BUILT_PRODUCTS_DIR}/sample1.tst
${BZIP2} -d  < ${BZIP2_SOURCE}/sample2.bz2 > ${BUILT_PRODUCTS_DIR}/sample2.tst
${BZIP2} -ds < ${BZIP2_SOURCE}/sample3.bz2 > ${BUILT_PRODUCTS_DIR}/sample3.tst
unset DYLD_LIBRARY_PATH

cmp ${BZIP2_SOURCE}/sample1.bz2 ${BUILT_PRODUCTS_DIR}/sample1.rb2 && printf "[PASS] 1\n" || printf "[FAIL] 1\n"
cmp ${BZIP2_SOURCE}/sample2.bz2 ${BUILT_PRODUCTS_DIR}/sample2.rb2 && printf "[PASS] 2\n" || printf "[FAIL] 2\n"
cmp ${BZIP2_SOURCE}/sample3.bz2 ${BUILT_PRODUCTS_DIR}/sample3.rb2 && printf "[PASS] 3\n" || printf "[FAIL] 3\n"
cmp ${BUILT_PRODUCTS_DIR}/sample1.tst ${BZIP2_SOURCE}/sample1.ref && printf "[PASS] 4\n" || printf "[FAIL] 4\n"
cmp ${BUILT_PRODUCTS_DIR}/sample2.tst ${BZIP2_SOURCE}/sample2.ref && printf "[PASS] 5\n" || printf "[FAIL] 5\n"
cmp ${BUILT_PRODUCTS_DIR}/sample3.tst ${BZIP2_SOURCE}/sample3.ref && printf "[PASS] 6\n" || printf "[FAIL] 6\n"

