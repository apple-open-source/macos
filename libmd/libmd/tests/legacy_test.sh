#!/bin/sh

# The .ref files in this directory are generated from lib/libmd/Makefile
# upstream.  This file is effectively a translation of the test target in the
# same directory.

DIR=$(dirname "$0")
failed=0

check()
{
	NUM=$1
	digest=$2

	if ${DIR}/${digest}driver | cmp -s ${DIR}/${digest}.ref -; then
		echo "ok ${NUM}"
	else
		failed=$((failed + 1))
		echo "not ok ${NUM}"
	fi
}

echo 1..8

check 1 md4
check 2 md5
check 3 sha0
check 4 sha1
check 5 sha224
check 6 sha256
check 7 sha384
check 8 sha512
exit $failed
