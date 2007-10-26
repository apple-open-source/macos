#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Template test script.
PROGNAME=smbfs-files-buster.sh

# Figure out where the scripts directory is. Allow external override
# so that the SimonSays wrappers can run these from symlinks be setting
# SCRIPTBASE explicitly.
SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: $PROGNAME SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

MOUNTPOINT=$(create_temp_dir "$0.mnt")
SCRATCH=$(create_temp_dir "$0.scratch")

failed=0
failtest()
{
    failed=`expr $failed + 1`
}

mount()
{
    case $SERVER in
	localhost|127.0.0.1)
	    if [ "$USERNAME" != "$LOGNAME" ]; then
		echo WARNING: SMB username it not the same as your login
		echo WARNING: this test will probably not work as expected
	    fi
	;;
    esac

    mount_smbfs //$USERNAME:$PASSWORD@$SERVER/$SHARE "$@" || \
	testerr $0 "failed to mount //$USERNAME@$SERVER/$SHARE"
}

setup_state()
{

    echo installing FilesBuster
    (
	set -e
	cd $SCRATCH
	$SCRIPTBASE/download-files-buster.sh > /dev/null | indent
	tar -xvzf FilesBuster.tgz > /dev/null | indent
    ) || testerr $0 "unable set up FilesBuster"

    FILESBUSTER=$SCRATCH/FilesBuster/FilesBuster/FilesBuster

    [ -d $FILESBUSTER/FilesBuster.app ] || \
	testerr $0 "FilesBuster.app is missing"

    echo mounting //$SERVER/$SHARE
    mount $MOUNTPOINT
}

remove_state()
{
    killall -TERM FilesBuster
    $ASROOT umount $MOUNTPOINT || $ASROOT umount -f $MOUNTPOINT
    [[ -d "$SCRATCH" ]] && rm -rf $SCRATCH
    [[ -d "$MOUNTPOINT" ]] && rm -rf $MOUNTPOINT
}

runapp()
{
    local application="$1"
    local appbase=$(basename $application)
    local appname=${appbase%%.app}

    shift
    echo running "$application/Contents/MacOS/$appname" "$@"
    "$application/Contents/MacOS/$appname" "$@"
}

run_files_buster()
{
    local result

    pushd $FILESBUSTER
    runapp ./FilesBuster.app -e -s FBMainScript \
	-d PRIMARY_TARGET=$MOUNTPOINT \
	-d FILESYSTEM_SMB \
	-d CLIENT_OS_LEOPARD
    result="$?"
    popd

    [[ $result == 0 ]]
}

register_cleanup_handler "remove_state > /dev/null 2>&1"
setup_state

run_files_buster || failtest

testok $0 $failed
