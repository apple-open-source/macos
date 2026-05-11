#!/bin/sh -eu

fails=0

: ${scriptdir:=$(dirname $(realpath "$0"))}
. "$scriptdir"/test_common.sh

## Smoke tests
1>&2 echo "# Test 'bzip2'"
mkfile sample
if ! bzip2 sample; then
	1>&2 echo "Failed to compress sample file"
	exit 1
elif [ ! -s sample.bz2 ]; then
	1>&2 echo "sample.bz2 expected, but not found:"
	1>&2 ls -l
	exit 1
fi

1>&2 echo "# Test 'bzip2 -d'"
if ! bzip2 -kd sample.bz2; then
	1>&2 echo "Failed to decompress sample.bz2"
	exit 1
elif ! checkfile sample; then
	1>&2 echo "bad sample file"
	exit 1
fi

rm sample

1>&2 echo "# Test 'bzip2 -cd'"
if ! bzip2 -cd sample.bz2 > sample_file; then
	1>&2 echo "Failed to decompress to stdout"
	exit 1
elif ! checkfile sample_file; then
	1>&2 echo "bad sample file from stdout"
	exit 1
fi

rm sample_file sample.bz2

## xattr tests
attr=com.apple.bzip2_test=yes

1>&2 echo "# Test 'bzip2'"
mkfile sample "$attr"
if ! bzip2 sample; then
	1>&2 echo "bzip2 failed to compress file with xattrs"
	exit 1
elif ! checkattrs sample.bz2 "$attr"; then
	1>&2 echo "sample.bz2 xattr not preserved"
	exit 1
fi

1>&2 echo "# Test 'bzip2 -d'"
if ! bzip2 -d sample.bz2; then
	1>&2 echo "bzip2 failed to decompress file with xattrs"
	exit 1
elif ! checkfile sample "$attr"; then
	1>&2 echo "bzip2 decompression with xattrs failed"
	exit 1
fi

bzip2 sample
checkattrs sample.bz2 "$attr"

1>&2 echo "# Test 'bzip2 -dc' with only stdout redirect"
if ! bzip2 -kdc sample.bz2 > sample; then
	1>&2 echo "bzip2 failed to decompress xattr file to stdout"
	exit 1
elif ! checkfile sample "$attr"; then
	1>&2 echo "sample output with xattrs not preserved"
	exit 1
fi

rm sample
1>&2 echo "# Test stdout pipe, won't preserve xattrs"
if ! bzip2 -kdc sample.bz2 | cat > sample; then
	1>&2 echo "bzip2 failed to decompress xattr file to a pipe"
	exit 1
elif ! checkfile sample; then
	1>&2 echo "sample output from pipe with xattrs failed"
	exit 1
fi

rm sample
1>&2 echo "# Test 'bzip2 -dc' with stdin redirect, should preserve xattrs"
if ! bzip2 -kdc < sample.bz2 > sample; then
	1>&2 echo "bzip2 decompress with both stdin and stdout redirected failed"
	exit 1
elif ! checkfile sample "$attr"; then
	1>&2 echo "sample output from both stdin/stdout redirected failed"
	exit 1
fi

rm sample
1>&2 echo "# Test a stdin pipe and stdout redirect, should not preserve xattrs"
if ! cat sample.bz2 | bzip2 -kdc > sample; then
	1>&2 echo "bzip2 decompress with stdin pipe failed"
	exit 1
elif ! checkfile sample; then
	1>&2 echo "sample output from both stdin pipe failed"
	exit 1
fi
