#!/bin/sh

# The .ref files in this directory are generated from lib/libmd/Makefile
# upstream.  This file is effectively a translation of the test target in the
# same directory.

DIR=$(dirname "$0")
failed=0

check()
{
	NUM=$1
	ofs=$2
	len=$3

	sed -n "${NUM}p" ${DIR}/file.out > ${NUM}.ref
	if ${DIR}/file_test ${DIR}/file.in ${ofs} ${len} | cmp -s ${NUM}.ref -; then
		echo "ok ${NUM}"
	else
		failed=$((failed + 1))
		echo "not ok ${NUM}"
	fi
}

echo 1..4

# Ensure whole file works.
check 1 0 0
# Check extracting the middle bits; in this case, lowercase alphabet.
check 2 26 26
# Check extracting limited length just from the start; in this case, uppercase
# alphabet.
check 3 0 26
# Check extracting from a position to the end (len=0).
check 4 26 0
exit $failed
