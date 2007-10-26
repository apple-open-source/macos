#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

# Usage: enforce-quota-support.sh USERNAME PASSWORD

# Basic script to exercise quota support. We create a temporary filesystem and
# probe the volume size using smbclient. After applying a quota on file blocks,
# we expect writing a file larger that the quota should fail.


USERNAME=${1:-local}
PASSWORD=${2:-local}

VOLUME=${VOLUME:-FOO}

QUOTA_BLOCKS_HARD=0
QUOTA_BLOCKS_SOFT=0
QUOTA_INODES_HARD=0
QUOTE_INODES_SOFT=0

DEBUGLEVEL=${DEBUGLEVEL:-1}
SMBCLIENT="/usr/bin/smbclient -d$DEBUGLEVEL -N -U$USERNAME%$PASSWORD"
CIFSDD="/usr/local/bin/cifsdd -d$DEBUGLEVEL -U$USERNAME%$PASSWORD"

failed=0
failtest()
{
    failed=$[$failed + 1]
}

cleanup()
{
    remove_smb_share $VOLUME
    $SCRIPTBASE/quota-image.sh destroy $VOLUME > /dev/null 2>&1
    true
}

update_quota()
{
    QUOTA_USER=$USERNAME \
    QUOTA_VOLUME=$VOLUME \
    QUOTA_BLOCKS_HARD=$QUOTA_BLOCKS_HARD \
    QUOTA_BLOCKS_SOFT=$QUOTA_BLOCKS_SOFT \
    QUOTA_INODES_HARD=$QUOTA_INODES_HARD \
    QUOTA_INODES_SOFT=$QUOTA_INODES_SOFT \
    EDITOR=$SCRIPTBASE/quota-edit.pl \
	$ASROOT edquota -u $USERNAME
}

echo destroying volume $VOLUME "(just in case)"
$SCRIPTBASE/quota-image.sh destroy $VOLUME 2>&1 | indent
echo creating volume $VOLUME
$SCRIPTBASE/quota-image.sh create $VOLUME 2>&1 | indent

echo updating Samba configuration
create_smb_share $VOLUME /Volumes/$VOLUME
register_cleanup_handler cleanup

# NOTE: quotas are given in 1K blocks
echo applying disk space quota
QUOTA_BLOCKS_SOFT=100 QUOTA_BLOCKS_HARD=100 update_quota | indent

echo testing for quota enforcement

(
    $CIFSDD bs=1024 count=200 \
	if=/dev/zero of=//localhost/$VOLUME/dummy  2>&1
) | grep --silent NT_STATUS_DISK_FULL

[[ "$?" = "0" ]] || failtest

echo applying file count quotas
QUOTA_INODES_SOFT=10 QUOTA_INODES_HARD=10 update_quota | indent

(
    for d in 1 2 3 4 5 6 7 8 9 10 11 12 ; do
	echo "md $d"
    done  | $SMBCLIENT //localhost/$VOLUME 2>&1
) | grep --silent NT_STATUS_DISK_FULL

[[ "$?" = "0" ]] || failtest

testok $0 $failed
