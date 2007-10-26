#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

# Usage: dfree-quota-support.sh USERNAME PASSWORD

# Basic script to exercise quota support. We create a temporary filesystem and
# probe the volume size using smbclient. After applying a quota on file blocks,
# the new volume size should match the quota.


USERNAME=${1:-local}
PASSWORD=${2:-local}

VOLUME=${VOLUME:-FOO}

ASROOT=sudo

QUOTA_BLOCKS_HARD=0
QUOTA_BLOCKS_SOFT=0
QUOTA_INODES_HARD=0
QUOTE_INODES_SOFT=0

DEBUGLEVEL=${DEBUGLEVEL:-1}
SMBCLIENT="/usr/bin/smbclient -d$DEBUGLEVEL -N -U$USERNAME%$PASSWORD"

failed=0
failtest()
{
    failed=$[$failed + 1]
}


add_share_hack()
{
    save_config_file /etc/smb.conf $(basename $0)
    cat > /etc/smb.conf <<EOF
[global]
    log level = $DEBUGLEVEL,quota:10
    debug uid = yes
    debug pid = yes
EOF

    create_smb_share $VOLUME /Volumes/$VOLUME
}

cleanup()
{
    restore_config_file /etc/smb.conf $(basename $0)
    remove_smb_share $VOLUME

    echo destroying volume $VOLUME
    $SCRIPTBASE/quota-image.sh destroy $VOLUME 2>&1 | indent
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

smbclient_probe()
{
    local expected=$1

    # Figure out how big smbclient thinks the volume is
    size=$( $SMBCLIENT //localhost/$VOLUME -c ls 2>&1 | \
		perl -ne 'if (/(\d+) blocks of size (\d+)/) {print ($1 * $2);}' )

    if [ -z "$size" ] ; then
	# Something went wrong .. run the connect again so we get a hint in the
	# script output.
	$SMBCLIENT //localhost/$VOLUME -c ls
    fi

    if [ -n "$expected" ] ; then
	echo expected size of $expected bytes, found $size bytes
	[ "$size" = "$expected" ]
    else
	echo volume size is $size bytes
	[ -n "$size" ]
    fi
}

echo destroying volume $VOLUME "(just in case)"
$SCRIPTBASE/quota-image.sh destroy $VOLUME 2>&1 | indent
echo creating volume $VOLUME
$SCRIPTBASE/quota-image.sh create $VOLUME 2>&1 | indent

echo updating Samba configuration
add_share_hack
register_cleanup_handler cleanup

echo probing volume size
smbclient_probe || failtest

# NOTE: quotas are given in 1K blocks
echo applying disk quota
QUOTA_BLOCKS_SOFT=100 update_quota | indent

echo probing volume size
smbclient_probe $[100 * 1024] || failtest

testok $0 $failed
