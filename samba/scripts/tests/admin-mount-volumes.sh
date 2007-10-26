#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Test seeing local volumes if you have are in the admin group.
#
# We need to test if the local volumes are being share by samba
# when the user is in the admin group. We also need to make sure
# that a normal user will not automaticlly see these same shares.
#
# We create and mount a disk image that should be accessible to
# users in the admin group through smb. We should be able to mount 
# that share as a user in the admin. We should not be able to mount 
# that share as guest. 

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}

.  $SCRIPTBASE/common.sh || exit 2
.  $SCRIPTBASE/disk-images.sh || exit 2

failed=0

USERNAME=${USERNAME:-local}
PASSWORD=${PASSWORD:-local}
DEBUGLEVEL=${DEBUGLEVEL:-4}
DISKIMAGESIZE=${DISKIMAGESIZE:-10485760}
DISKIMAGE=${DISKIMAGE:-admin-volume.dmg}
SHAREPT=${SHAREPT:-admin-volume}
MOUNTPT=${MOUNTPT:-/tmp/mp}
MOUNTPTGUEST=${MOUNTPTGUEST:-/tmp/mp-guest}

SERVER=${SERVER:-localhost}
SMBURL=${SMBURL:-//$USERNAME:$PASSWORD@$SERVER/$SHAREPT}
SMBURLGUEST=${SMBURLGUEST:-//guest:@$SERVER/$SHAREPT}

# Print out which test failed
failtest()
{
	echo "*** TEST FAILED $1***"
    failed=`expr $failed + 1`
}

# Just clean up everything when we are done
remove_state()
{
	/sbin/umount -f $MOUNTPT 2>/dev/null ||
		echo "Cleanup failed to unmount volume $MOUNTPT"
	rmdir $MOUNTPT 2>/dev/null || \
		echo "Cleanup failed to remove mount point $MOUNTPT"
	if [ -d $MOUNTPTGUEST ]; then
		/sbin/umount -f $MOUNTPTGUEST 2>/dev/null
		rmdir $MOUNTPTGUEST 2>/dev/null || \
			echo "Cleanup failed to remove mount point $MOUNTPTGUEST"
	fi
	unmount_disk_image $DISKIMAGE 1>/dev/null 2>/dev/null || \
		echo "Cleanup failed to unmount disk image $DISKIMAGE"
	true
}

# Clean up anything left over from a failed setup
setup_failed()
{
    remove_state
    testerr $0 "$1"
}

# Create and mount a disk image, admin should be able to mount it
# on the clean system. Create the need mount points for the admin
# mount and guest mount
setup_state()
{
	make_disk_image $DISKIMAGESIZE $DISKIMAGE 1>/dev/null 2>/dev/null || \
		setup_failed "Setup failed to create disk image $DISKIMAGE"

	mount_disk_image $DISKIMAGE 1>/dev/null 2>/dev/null || \
		setup_failed "Setup failed to mount disk image $DISKIMAGE"

	mkdir $MOUNTPT 2>/dev/null || \
		setup_failed "Setup failed to make directory $MOUNTPT"

	mkdir $MOUNTPTGUEST 2>/dev/null || \
		setup_failed "Setup failed to make directory $MOUNTPTGUEST"
   true
}

# See if we can mount it using smb. Since we are mounting with admin creds
# this should succeed. This means the items under /Volumes are 
# being shared for admin users.
do_admin_test()
{
	mount -t smbfs $SMBURL $MOUNTPT 2>/dev/null || \
		failtest "admin couldn't mount the volume"
    true
}

# See if we can mount it using smb. Since we are mounting with guest creds
# this should failed. This means the items under /Volumes are 
# not being shared for non-admin users.
do_guest_test()
{
	guestmnt=0

	mount -t smbfs $SMBURLGUEST $MOUNTPTGUEST 2>/dev/null || \
		guestmnt=1

	if [ $guestmnt -eq 0 ]; then
		failtest "guest could mount the volume"
	fi
	true
}

setup_state
register_cleanup_handler remove_state

do_admin_test || failtest
do_guest_test || failtest

testok $0 $failed
