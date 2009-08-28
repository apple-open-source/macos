#! /bin/bash

# Basic script to test mapping from user and group IDs to SIDs
# Copyright (C) 2007 Apple Inc. All rights reserved.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}

.  $SCRIPTBASE/common.sh
.  $SCRIPTBASE/directory-services.sh

ASROOT=sudo
failed=0

# filter that extracts things that look like SIDs.
sidfilter()
{
    perl -ne '/(S-[0123456789-]+)/ && print "$1\n";'
}

uid_list=$(create_temp_file $0 uid) || testerr $0 "can't create temp file"
gid_list=$(create_temp_file $0 gid) || testerr $0 "can't create temp file"
user_sid_list=$(create_temp_file $0 usersid) || \
		testerr $0 "can't create temp file"
group_sid_list=$(create_temp_file $0 groupsid) || \
		testerr $0 "can't create temp file"

machine_sid=$($ASROOT net getlocalsid | sidfilter )

failtest()
{
    echo "*** TEST FAILED ***"
    failed=`expr $failed + 1`
}

cleanup()
{
    rm -f $uid_list $gid_list $user_sid_list $group_sid_list
}

trap "cleanup 2>/dev/null" 0 1 2 3 15

unix_ids_match()
{
    # Hack around signed vs. unsigned wonkiness
    if [ "$1" = "-2" -a "$2" = "2147483647" ]; then
	true
    elif [ "$2" = "-2" -a "$1" = "2147483647" ]; then
	true
    elif [ "$1" = "$2" ]; then
	if [ "$1" = "" ]; then
	    false
	else
	    true
	fi
    else
	false
    fi
}

group_sids_match()
{
    if [ "$1" = "" -o "$2" = "" ]; then
	false
    elif [ "$1" = "$2" ]; then
	true
    elif [ x$(wbinfo --sid-to-gid=$1) = x$(wbinfo --sid-to-gid=$2) ] ; then
	# SIDs are textually different, but maybe they resolve to the same
	# thing 
	true
    else
	false
    fi
}

user_sids_match()
{
    if [ "$1" = "" -o "$2" = "" ]; then
	false
    elif [ "$1" = "$2" ]; then
	true
    elif [ x$(wbinfo --sid-to-uid=$1) =  x$(wbinfo --sid-to-uid=$2) ] ; then
	# SIDs are textually different, but maybe they resolve to the same
	# thing 
	true
    else
	false
    fi
}

pdb_user_sid()
{
    $ASROOT pdbedit -d0 -v -u $1 | awk -F: '/User SID/{print $2}' | \
	sidfilter
}

pdb_group_sid()
{
    $ASROOT pdbedit -d0 -v -u $1 | awk -F: '/Group SID/{print $2}' | \
	sidfilter
}

check_well_known_group_sid()
{
    local sid="$1"
    local expected_group="$2"

    echo checking that well-known group SID $sid is group $expected_group

    expected_gid=$(ds_lookup_group_gid $expected_group) || \
	testerr $0 "unable to get GID for group $expected_group"

    wb_gid=$(wbinfo --sid-to-gid=$sid)
    if [ $? -ne 0 -o "$wb_gid" != "$expected_gid" ]; then
	failtest
	if [ "$wb_gid" ]; then
	    echo $sid mapped to GID $wb_gid, but expected $expected_gid
	else
	    echo failed to map $sid to expected GID $expected_gid
	fi
    fi
}

check_well_known_group_rid()
{
    local sid="$1"
    local rid="$2"
    local expected_group="$3"

    check_well_known_group_sid "${sid}-${rid}" $expected_group
}

if [ -e /var/samba/idmap_cache.tdb ]; then
    $ASROOT tdbtool /var/samba/idmap_cache.tdb erase || \
	    testerr $0 "failed to clear the idmap cache"
fi

echo testing existing fixed group SID mappings

# jpeach$ dscl /Search -list /Groups SMBSID
# _clamav         S-1-5-21-182 S-1-5-21-183
# _cvs            S-1-5-21-172
# ...

$DSSEARCH -list /Groups SMBSID | while read groupname sidlist; do
	# Skip wonky groups that will not resolve consistently.
	# _amavisd  SMBSID = ("S-1-5-21-183")
	# _clamav   SMBSID = ("S-1-5-21-182", "S-1-5-21-183")
	case $groupname in
	    _clamav|_amavisd) continue;
	esac 

	for sid in $sidlist ; do
		check_well_known_group_sid $sid $groupname
	done 
done

# SID: S-1-5-domain-512 - Domain Admins
check_well_known_group_rid $machine_sid 512 admin

# SID: S-1-5-domain-513 - Domain Users
check_well_known_group_rid $machine_sid 513 staff

# SID: S-1-5-domain-514 - Domain Guests
# This should match "guest user = unknown" in /etc/smb.conf
check_well_known_group_rid $machine_sid 514 unknown

# Flush the cache to prevent the previous lookups producing spurious failures,
# eg. we can find the domain "nobody" SID where we want the "well-known" nobody
# SID.
if [ -e /var/samba/idmap_cache.tdb ]; then
    $ASROOT tdbtool /var/samba/idmap_cache.tdb erase || \
	    testerr $0 "failed to clear the idmap cache"
fi

$ASROOT killall -TERM winbindd

echo searching for user IDs
ds_search_user_ids > $uid_list

echo searching for group IDs
ds_search_group_ids > $gid_list

shopt -s extglob

echo mapping $(count_lines $uid_list) user IDs
while read uid ; do

    # -1 is not a valid UID, we don't have to map it
    if [ "$uid" = "-1" ]; then
	continue
    fi

    # For record with spaces, we don't parse the UID out right, ignore them.
    case $uid in
    	+([0-9])) ;;
    	-+([0-9])) ;;
	*)
	echo "skipping invalid uid ($uid)"
	continue
	;; 
    esac

    echo trying uid=$uid
    username=$(ds_lookup_user_name $uid) || testerr $0 "no name for user $uid"
    primary_gid=$(ds_user_primary_group $username) || \
		    testerr $0 "no primary group ID for $username"
    group=$(ds_lookup_group_name $primary_gid)

    context="user $username($uid), primary group $group($primary_gid)"

    # Test that winbind has a SID for this uid
    wb_usid=$(wbinfo -U $uid)
    if [ $? -ne 0 -o "$wb_usid" = "" ]; then
	failtest
	echo $context
	echo "unable to map SID for user $username (uid $uid)"
	continue
    fi

    wb_uid=$(wbinfo --sid-to-uid=$wb_usid)
    if ! unix_ids_match $wb_uid $uid ; then
	failtest
	echo $context
	echo mismatched user ID for $username
	echo "idmap gave:  " $wb_uid
	echo "expected:    " $uid
    fi

    user_sid=$(pdb_user_sid $username) ||  \
		    testerr $0 "no user SID for $username"
    group_sid=$(pdb_group_sid $username) || \
		    testerr $0 "no group SID for $username"

    # Make sure the the idmap module agrees with the SAM module
    if ! user_sids_match $user_sid $wb_usid ; then
	failtest
	echo $context
	echo mismatched user SID for $username
	echo "idmap gave:  " $wb_usid
	echo "passdb gave: " $user_sid
    fi

    wb_gsid=$(wbinfo --gid-to-sid=$primary_gid $username)

    # Make sure the the idmap module agrees with the SAM module
    if ! group_sids_match $group_sid $wb_gsid ; then
	failtest
	echo $context
	echo mismatched group SID for $username
	echo "idmap gave:  " $wb_gsid
	echo "passdb gave: " $group_sid
    fi

    echo $uid $wb_usid >> $user_sid_list
done < $uid_list

echo mapping $(count_lines $gid_list) group IDs
while read gid ; do

    # -1 is not a valid GID, we don't have to map it
    if [ "$gid" = "-1" ]; then
	continue
    fi

    wb_gsid=$(wbinfo -G $gid)
    if [ $? -ne 0 -o "$wb_gsid" = "" ]; then
	failtest
    fi

    wb_gid=$(wbinfo --sid-to-gid=$wb_gsid)
    if [ $? -ne 0 -o "$wb_gid" != "$gid" ]; then
	echo failed to reverse map $wb_gsid, got \"$wb_gid\" instead of $gid
	failtest
    fi

    echo $gid $wb_gsid >> $group_sid_list
done < $gid_list

echo checking that uid and user list lists match
if [ $(count_lines $user_sid_list) -ne $(count_lines $uid_list) ]; then
    failtest
fi

echo checking that gid and group list lists match
if [ $(count_lines $group_sid_list) -ne $(count_lines $gid_list) ]; then
    failtest
fi

testok $0 $failed
