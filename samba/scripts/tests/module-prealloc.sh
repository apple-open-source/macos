#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to test the vfs_prealloc module.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

TESTUSER=${TESTUSER:-local}
TESTPASSWD=${TESTPASSWD:-local}
CIFSDD=${CIFSDD:-/usr/local/bin/cifsdd}
DEBUGLEVEL=${DEBUGLEVEL:-1}

ASROOT=sudo

SHAREPATH=/tmp/$(basename $0)

SERVER=localhost
SHARE=PreallocTest
FILETYPE=mpeg

testfile=/tmp/$(basename $0)-$$.$FILETYPE
tmpfile=/tmp/$(basename $0)-$$.verify

failed=0

failtest()
{
    echo "*** FAILED ***"
    failed=`expr $failed + 1`
}

# Set up a Samba share.
rm -rf $SHAREPATH
mkdir -p $SHAREPATH
chmod 777 $SHAREPATH

$ASROOT net usershare add $SHARE $SHAREPATH prealloc S-1-1-0:F guest_ok=y \
    "read only = no" \
    "vfs objects = prealloc" \
    "prealloc:msglevel = 0" \
    "prealloc:$FILETYPE = 10M"

cleanup()
{
    echo Removing temporary test state

    $ASROOT net usershare delete $SHARE
    rm -f $tmpfile $testfile
    rm -rf $SHAREPATH
}

# Allow users to set NOCLEANUP to leave the temporary test state around.
if [ -z "$NOCLEANUP" ]; then
    register_cleanup_handler cleanup
fi

copyfile()
{
    $CIFSDD --debuglevel=$DEBUGLEVEL \
            -U "$TESTUSER"%"$TESTPASSWD" \
            "$@"
}

# This is not the greatest test in the world. We just set up the prealloc
# module and write/read/verify a file that ought to trigger a preallocation.
# The only way to verify that a preallocation is being attempted is to tail the
# log file or user dtrace.
#
# The log message looks like this:
#   prealloc: preallocating xxx (fd=25) to yyy  bytes

echo Creating an initial test file
dd if=/dev/random of=$testfile bs=4096 count=1 > /dev/null 2>&1

echo Writing a test file
copyfile of=//$SERVER/$SHARE/$(basename $testfile) \
	 if=$testfile > /dev/null 2>&1 || failtest

echo Reading it back
copyfile if=//$SERVER/$SHARE/$(basename $testfile) \
	 of=$tmpfile > /dev/null 2>&1 || failtest

echo Checking it is the same
files_are_the_same $testfile $tmpfile  > /dev/null 2>&1|| failtest

testok $0 $failed

