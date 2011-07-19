#!/bin/sh

template="/usr/local/libexec/openssh/regression-tests"
workingdir="/private/tmp/ssh_regression_tests_$$"
mkdir "${workingdir}"

ditto "${template}" "${workingdir}"
cd "${workingdir}"

export TEST_SHELL=/bin/sh
export OBJ=`pwd`

make

