#!/bin/sh -eu

: ${scriptdir:=$(dirname $(realpath "$0"))}
. "$scriptdir"/test_common.sh

corrupt() {
	local file

	file=$1
	if [ ! -s "$file" ]; then
		1>&2 echo "$file: does not exist"
		return 1
	fi

	dd if=/dev/zero of="$file" bs=1 count=1 oseek=3 conv=notrunc
}

mkfile sample
bzip2 sample

1>&2 echo "Corrupting our sample file"
corrupt sample.bz2
if bzip2 -d sample.bz2; then
	1>&2 echo "Failed to corrupt the sample file"
	exit 1
fi

1>&2 echo "# Test 'bzip2recover'"
if ! bzip2recover sample.bz2; then
	1>&2 echo "Failed to recover the sample file"
	exit 1
elif ! checkattrs rec00001sample.bz2; then
	1>&2 echo "Recovered file unexpectedly ended up with xattrs"
	exit 1
fi

rm -f rec*sample.bz2 sample.bz2

attrname=com.apple.bzip2recover_test
attrval=yes
attr="$attrname"="$attrval"

mkfile sample "$attr"
bzip2 sample
corrupt sample.bz2
checkattrs sample.bz2 "$attr"

1>&2 echo "# Test 'bzip2recover' with xattrs"
if ! bzip2recover sample.bz2; then
	1>&2 echo "Failed to recover the xattr-laden sample file"
	exit 1
elif ! checkattrs rec00001sample.bz2 "$attr"; then
	1>&2 echo "Recovered file unexpectedly ended up without xattrs"
	exit 1
fi

rm -f rec*sample.bz2 sample.bz2

dd if=/dev/random of=sample bs=4M count=1 conv=sync
xattr -w "$attrname" "$attrval" sample
bzip2 sample
corrupt sample.bz2
checkattrs sample.bz2 "$attr"

1>&2 echo "# Test multi-block recovery with xattrs"
if ! bzip2recover sample.bz2; then
	1>&2 echo "Failed to recover the xattr-laden many-block sample file"
	exit 1
fi

rm -f sample.bz2
for f in rec*sample.bz2; do
	if ! checkattrs "$f" "$attr"; then
		1>&2 echo "$f is missing the xattr, but all files expected to have it"
		exit 1
	fi

	rm -f "$f"
done
