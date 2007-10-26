#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to build Samba on a SMB volume.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
SRCROOT=$(cd $SCRIPTBASE/.. && pwd)
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: smbfs-samba-compile.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
# NOTE: $USERNAME must match the user this script is running as, otherwise we
# won't get access to the mountpoint.
# eg. sudo -u local ./scripts/tests/smbfs-samba-compile.sh  \
#    localhost local local local
USERNAME=${3:-$LOGNAME}
PASSWORD=${4:-local}

failed=0

failtest() {
	failed=`expr $failed + 1`
}

mountpoint=/tmp/$(basename $0).$$

mount()
{
    mount_smbfs //$USERNAME:$PASSWORD@$SERVER/$SHARE "$@"
}

cleanup()
{
    rm -rf $mountpoint/$(basename $SRCROOT)
    umount $mountpoint || ( sleep 1 ; $ASROOT umount -f $mountpoint)
    rm -rf $mountpoint
}

mkdir $mountpoint || testerr failed to create mount point
chmod 777 $mountpoint
mount $mountpoint || testerr failed to mount //$SERVER/$SHARE

register_cleanup_handler cleanup

rsync --archive $SRCROOT $mountpoint

pushd $mountpoint/$(basename $SRCROOT)
SRCROOT=. ./scripts/apply-patches.sh > /dev/null || failtest
./scripts/localbuild.sh > /dev/null || failtest
popd

testok $0 $failed
