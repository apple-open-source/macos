#!/bin/bash
#
# Copyright (c) 2006 - 2007 Apple Inc. All rights reserved.
#
# This script walks /Users and /Groups in an Open Directory master looking
# for SID attributes in each record.  If it encounters a record that lacks
# the SID attribute, it assigns one based on the following algorithm:
#
#	if user {
#		RID = (UserID << 1) + 1000;
#	} else {
#		RID = (GroupID << 1) + 1001;
#	}
#
#	SID = ${DomainSID}-${RID}
#
# This is script is used by XSan to assign SIDs to users and groups. This test
# verifies that the default mapping produced by winbindd is compatible with
# XSan.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}

.  $SCRIPTBASE/common.sh
.  $SCRIPTBASE/directory-services.sh

ASROOT=sudo
failed=0

failtest()
{
    echo "*** TEST FAILED ***"
    failed=`expr $failed + 1`
}

cleanup() {
	stty echo
	testok $0 $failed
}

extractDomainSID() {

	echo "${output}" | (
		while read line; do
			if [ "${line}" = "<key>SID</key>" ]; then
				read sid_line
				echo "${sid_line}" | \
					sed -e 's,<string>,,g' \
					    -e 's,<.*$,,g'
				exit 0
			fi
		done
	)
}

sidfilter()
{
        perl -ne '/(S-[0123456789-]+)/ && print "$1\n";'
}

nUsersTotal=0
nUsersUpdated=0
nUsersFailed=0

nGroupsTotal=0
nGroupsUpdated=0
nGroupsFailed=0

trap cleanup 1 2 3 13 15

if [ $# != 0 ]; then
	usage
fi

#
# Verify that we can talk to Open Directory.
#
output=`${DSCL} -read / 2>&1`
if [ "${output}" = "Data source (${OpenDirectoryMetaNode}) is not valid." ]; then
	testerr $0 "Unable to connect to Open Directory"
fi

#
# Fetch the Domain SID.
#
output=`${DSCL} -read /Config/CIFSServer XMLPlist 2>&1`
dsKey=`echo ${output} | cut -d ' ' -f 1`
if [ "${dsKey}" != "XMLPlist:" ]; then
#jpeach	echo "You must create a Domain SID before assigning static SIDs." \
#jpeach	    >&2
#jpeach#exit 1
#jpeachfi
	echo "Using Samba domain SID"
	domainSID=$($ASROOT net getlocalsid | sidfilter )
else
	domainSID=`extractDomainSID "${output}"`
fi

if [ -e /var/samba/idmap_cache.tdb ]; then
    $ASROOT tdbtool /var/samba/idmap_cache.tdb erase || \
	    testerr $0 "failed to clear the idmap cache"
fi

#
# Get the list of users.
#
userList=`$DSSEARCH -list /Users 2>&1 | tr '\n' ' '`

for user in ${userList}; do
	nUsersTotal=$((${nUsersTotal} + 1))
	output=`$DSSEARCH -read /Users/${user} SMBSID 2>&1`
	dsKey=`echo ${output} | cut -d ' ' -f 1`
	if [ "${dsKey}" = "SMBSID:" ]; then
		# User already has a SID.
		continue
	fi
	output=`$DSSEARCH -read /Users/${user} UniqueID 2>&1`
	dsKey=`echo ${output} | cut -d ' ' -f 1`
	if [ "${dsKey}" != "UniqueID:" ]; then
		echo "User ${user} lacks an Open Directory UID." >&2
		continue;
	fi
	userID=`echo ${output} | cut -d ' ' -f 2`
	# Computed RID for user:
	#	(UID << 1) + WellKnownRidBase[1000]
	rid=$(((${userID} * 2) + 1000))
	newSID="${domainSID}-${rid}"
	sambaSID=$(wbinfo --uid-to-sid=$userID)
	if [ "$sambaSID" != "$newSID" ]; then
		failtest
		echo "XSan SID for $user is:" $newSID | indent
		echo "SMB SID for $user is: " $sambaSID | indent
		nUsersFailed=$((${nUsersFailed} + 1))
	else
		nUsersUpdated=$((${nUsersUpdated} + 1))
	fi
done

#
# Get the list of groups.
#
groupList=`$DSSEARCH -list /Groups 2>&1 | tr '\n' ' '`

for group in ${groupList}; do
	nGroupsTotal=$((${nGroupsTotal} + 1))
	output=`${DSCL} -read /Groups/${group} SMBSID 2>&1`
	dsKey=`echo ${output} | cut -d ' ' -f 1`
	if [ "${dsKey}" = "SMBSID:" ]; then
		# Group already has a SID.
		continue
	fi
	output=`${DSCL} -read /Groups/${group} PrimaryGroupID 2>&1`
	dsKey=`echo ${output} | cut -d ' ' -f 1`
	if [ "${dsKey}" != "PrimaryGroupID:" ]; then
		nGroupsFailed=$((${nGroupsFailed} + 1))
		continue
	fi
	groupID=`echo ${output} | cut -d ' ' -f 2`
	# Computed RID for group:
	#	(GID << 1) + WellKnownRidBase[1000] + 1
	rid=$(((${groupID} * 2) + 1000 + 1))
	newSID="${domainSID}-${rid}"
	sambaSID=$(wbinfo --gid-to-sid=$groupID)
	if [ "$sambaSID" != "$newSID" ]; then
		failtest
		echo "XSan SID for $user is:" $newSID | indent
		echo "SMB SID for $user is: " $sambaSID | indent
		nGroupsFailed=$((${nGroupsFailed} + 1))
	else
		nGroupsUpdated=$((${nGroupsUpdated} + 1))
	fi
done

if [ $((${nUsersUpdated} + ${nGroupsUpdated})) != 0 ]; then
	echo ""
fi

echo "Updated records for ${nUsersUpdated} out of ${nUsersTotal} users (${nUsersFailed} failed)."

echo "Updated records for ${nGroupsUpdated} out of ${nGroupsTotal} groups (${nGroupsFailed} failed)."

testok $0 $failed
