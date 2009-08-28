#!/bin/bash
cd $(dirname $0) && tclconfig=$(pwd) && cd ..
find ${1:-.} -name 'tcl.m4' ! -path "./tclconfig/*" \
	-exec cp -p "${tclconfig}/tcl.m4" {} \;
find ${1:-.} \( -name 'configure.in' -or -name 'configure.ac' \) \
	-exec grep -q '^TEA_INIT(' {} \; \
	-exec "${tclconfig}/fix_tea_vers.sh" {} \;
find ${1:-.} -name 'configure' \
	-exec grep -E -q 'TEA_(INIT|VERSION)' {} \; \
	-exec "${tclconfig}/reconf.sh" {} \;
