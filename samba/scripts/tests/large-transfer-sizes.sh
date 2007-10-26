#! /bin/bash
# Portions Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Basic script to make verify that the server supports large transfer sizes

# Derived from source/script/tests/test_cifsdd.sh in the SAMBA_4_0 branch
# of the svn.samba.org repository.


SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: large_transfer_sizes.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

DD=/usr/local/bin/cifsdd

DEBUGLEVEL=1

failed=0

failtest() {
	failed=`expr $failed + 1`
}

runcopy() {
	message="$1"
	shift
	
	echo $message
	vrun $DD $CONFIGURATION --debuglevel=$DEBUGLEVEL \
	    -U "$USERNAME"%"$PASSWORD" \
	    "$@"
}

compare() {
    cmp "$1" "$2" > /dev/null 2>&1
}

sourcepath=tempfile.src.$$
destpath=tempfile.dst.$$

# Create a source file with arbitrary contents. The size should be larger than
# any of the block sizes we want to test. We make sure it is not an even
# multiple.
dd if=/dev/random of=$sourcepath bs=1024 count=400
dd if=/dev/random seek=$[1024 * 400] of=$sourcepath bs=1 count=30

ls -l $sourcepath

for bs in 512 4k 48k 64k 127k ; do

echo "Testing $bs block size ..."

# Check whether we can do a round trip
runcopy "Testing local -> remote copy" \
	    if=$sourcepath of=//$SERVER/$SHARE/$sourcepath bs=$bs || failtest
runcopy "Testing remote -> local copy" \
	    if=//$SERVER/$SHARE/$sourcepath of=$destpath bs=$bs || failtest
compare $sourcepath $destpath || failtest

# Check that copying within the remote server works
runcopy "Testing local -> remote copy" \
	    if=$sourcepath of=//$SERVER/$SHARE/$sourcepath bs=$bs || failtest
runcopy "Testing remote -> remote copy" \
	    if=//$SERVER/$SHARE/$sourcepath of=//$SERVER/$SHARE/$destpath bs=$bs || failtest
runcopy "Testing remote -> local copy" \
	    if=//$SERVER/$SHARE/$destpath of=$destpath bs=$bs || failtest
compare $sourcepath $destpath || failtest

done

rm -f $sourcepath $destpath

testok $0 $failed
