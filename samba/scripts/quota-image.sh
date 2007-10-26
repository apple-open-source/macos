#! /bin/bash

# Copyright (C) Apple Computer, Inc. All rights reserved.

# quota-image.sh - Create and destroy quota-enabled disk images

usage()
{
    cat <<EOF
Usage: quota-image.sh OPERATION NAME

Supported OPERATION types:
    create	Create and mount IMAGE
    destroy	Unmount and destroy IMAGE
EOF
}

if [ $# != "2" ] ; then
    usage
    exit 1;
fi

OPERATION="$1"
IMAGE="$2"

# Make sure that IMAGE is foo.dmg
case $IMAGE in
    *.dmg) ;;
    *) IMAGE="$IMAGE.dmg" ;;
esac

VOLNAME=$(echo $IMAGE | sed '-es/.dmg//' | tr 'a-z' 'A-Z')
SIZE=${SIZE:-20m}

ASROOT=sudo
ME=$(whoami)

make_disk_image()
{
    echo creating disk image
    hdiutil create -size $SIZE -fs HFSJ -volname $VOLNAME $IMAGE
}

mount_disk_image()
{
    echo mounting disk image
    hdiutil attach $IMAGE -readwrite -mount required -owners on
    $ASROOT mount -u -o owners,perm /Volumes/$VOLNAME

    # tell spotlight not not index this
    $ASROOT mdutil -v -i off /Volumes/$VOLNAME
}

enable_quotas()
{
    echo enabling user and group quotas
    $ASROOT touch /Volumes/$VOLNAME/.quota.ops.user
    $ASROOT touch /Volumes/$VOLNAME/.quota.ops.group
    $ASROOT quotacheck -ug /Volumes/$VOLNAME
    $ASROOT quotaon -ugv /Volumes/$VOLNAME
}

cleanup()
{
    echo destroying disk image
    $ASROOT quotaoff -ugv /Volumes/$VOLNAME

    # This does whatever magic is needed to stop mds and fseventsd.
    hdiutil detach /Volumes/$VOLNAME
    rm -f $IMAGE
}

case $OPERATION in
    create)
	# Note that we don't trap on exit (ie. 0)
	trap cleanup 1 2 3 15
	make_disk_image && mount_disk_image && enable_quotas
	df -h /Volumes/$VOLNAME
	$ASROOT chmod 777 /Volumes/$VOLNAME
	$ASROOT repquota -u -v /Volumes/$VOLNAME
	exit 0

	;;
    destroy)
	cleanup
	;;
    *)
	usage
	exit 1
	;;
esac
