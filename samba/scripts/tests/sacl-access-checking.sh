#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to test SACL access checks.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}

.  $SCRIPTBASE/common.sh || exit 2
.  $SCRIPTBASE/directory-services.sh || exit 2

#QUIET=">/dev/null 2>&1"

TESTUSER=${TESTUSER:-smbtest}
TESTPASSWD=${TESTPASSWD:-smbtest}
DEBUGLEVEL=${DEBUGLEVEL:-1}

ASROOT=${ASROOT:-sudo}
SERVER=localhost

failed=0

failtest()
{
    echo "*** TEST FAILED ***"
    failed=`expr $failed + 1`
}

cleanup()
{
    ds_delete_user smballow > /dev/null 2>&1
    ds_delete_user smbdeny > /dev/null 2>&1
    ds_delete_group com.apple.access_smb

    exit
}

# Allow users to set NOCLEANUP to leave the temporary test state around.
if [ -z "$NOCLEANUP" ]; then
    register_cleanup_handler cleanup
fi

ds_delete_user smballow> /dev/null 2>&1
ds_delete_user smbdeny> /dev/null 2>&1
ds_delete_group com.apple.access_smb > /dev/null 2>&1

ds_create_user smballow smballow || \
		   testerr $0 "failed to add user smballow"
ds_create_user smbdeny smbdeny || \
		   testerr $0 "failed to add user smbdeny"
ds_create_user smbdeny smbdeny || \
		   testerr $0 "failed to add user smbdeny"

poke()
{
    local user="$1"
    /usr/bin/smbclient -d$DEBUGLEVEL -U$user%$user \
	-N -L localhost > /dev/null
}

echo SACL is not active, we should succeed
poke smbdeny || failtest

ds_create_group com.apple.access_smb

echo SACL is active, but we are not added, we should fail
poke smbdeny && failtest

# We need to test the allow case with a different user account because the
# group membership test is cached in the kernel and in DS.
ds_add_user_to_group smballow $(ds_lookup_group_gid com.apple.access_smb)

echo SACL is active, and we are added, we should succeeed
poke smballow || failtest

testok $0 $failed
