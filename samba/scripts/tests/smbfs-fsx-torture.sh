#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to run fsx and fstorture against a server.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

FSX=~fs//bin/fsx
FSTORTURE=~fs/bin/fstorture

if [ $# -lt 2 ]; then
cat <<EOF
Usage: fsx-torture.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

ASROOT=

FSTORTURE_NPROC=2
FSTORTURE_TIME=10min
FSTORTURE_DIR1=smb1
FSTORTURE_DIR2=smb2
FSTORTURE_OPTIONS="no_stats windows_volume"
FSTORTURE_DURATION=1h

FSX_FILE=smbfsx
FSX_DURATION=1h

failed=0

failtest() {
	failed=`expr $failed + 1`
}

mountpoint=/tmp/$(basename $0).$$

mount()
{
    if [ "$USERNAME" != "$LOGNAME" ]; then
	echo WARNING: SMB username it not the same as your login
	echo WARNING: this test will probably not work as expected
    fi

    $ASROOT mount_smbfs //$USERNAME:$PASSWORD@$SERVER/$SHARE "$@"
}

cleanup()
{
    # TODO: we really should be more precise here ...
    killall -TERM fstorture fsx
    rm -rf $mountpoint/$FSTORTURE_DIR1 \
	    $mountpoint/$FSTORTURE_DIR2 \
	    $mountpoint/$FSX_FILE

    $ASROOT umount $mountpoint
    $ASROOT umount -f $mountpoint

    rm -rf $mountpoint
}

fstorture()
{
    mkdir $mountpoint/$FSTORTURE_DIR1
    mkdir $mountpoint/$FSTORTURE_DIR2

    if [ -d $mountpoint/$FSTORTURE_DIR1 -a \
	 -d $mountpoint/$FSTORTURE_DIR2 ]; then
	$FSTORTURE \
	    $mountpoint/$FSTORTURE_DIR1 \
	    $mountpoint/$FSTORTURE_DIR2 \
	    $FSTORTURE_NPROC \
	    $FSTORTURE_OPTIONS \
	    -t $FSTORTURE_DURATION
    else
	failtest
    fi
}

fsx()
{
    touch $mountpoint/$FSX_FILE

    if [ -f $mountpoint/$FSX_FILE ]; then
	$FSX $mountpoint/$FSX_FILE -d $FSX_DURATION
    else
	failtest
    fi
}

mkdir $mountpoint || testerr failed to create mount point
chmod 777 $mountpoint
mount $mountpoint || testerr failed to mount //$SERVER/$SHARE

register_cleanup_handler "cleanup 2>/dev/null"

fstorture &
fsx & 

wait
testok $0 $failed
