#! /bin/bash

# Shell functions to manipulate Directory Services
# Copyright (C) 2007 Apple Inc. All rights reserved.

DSCL=${DSCL:-"/usr/bin/dscl ."}
DSSEARCH=${DSSEARCH:-"/usr/bin/dscl /Search"}
DSEDITGROUP=${DSEDITGROUP:-"/usr/sbin/dseditgroup -n ."}
ASROOT=${ASROOT:-sudo}

# List the local users, sorted by ID.
ds_list_user_ids()
{
    $DSCL -list /Users UniqueID | awk '{print $2}' | sort -n
}

# Find all the known user IDs
ds_search_user_ids()
{
    $DSSEARCH -list /Users UniqueID | awk '{print $2}' | sort -n
}

# Find all the known user IDs
ds_search_group_ids()
{
    $DSSEARCH -list /Groups PrimaryGroupID | awk '{print $2}' | sort -n
}

ds_list_group_ids()
{
    $DSCL -list /Groups PrimaryGroupID | awk '{print $2}' | sort -n
}

# True if the given username exists
ds_user_exists()
{
    $DSCL -list /Users/$1 > /dev/null 2>&1
}

# Return the user's primary group if passed a user name. For some reason
# /Search doesn't always find attributes and we need to look in /Local.
ds_user_primary_group()
{
    gid=$($DSSEARCH -read /Users/$1 PrimaryGroupID | awk '{print $2}')
    if [ "$gid" = "" ]; then
	gid=$($DSCL -read /Users/$1 PrimaryGroupID | awk '{print $2}')
    fi

    if [ "$gid" = "" ]; then
	false
    else
	echo $gid
    fi
}

ds_find_next_uid()
{
    local highest=$(ds_list_user_ids | tail -1)
    echo $[$highest + 1]
}

ds_enable_user_for_smb()
{
    local user="$1"
    local passwd="$2"

    # For some reason, we need to set the password both before and
    # after flipping the hashes. Go figure.
    $ASROOT $DSCL -passwd /Users/$user $passwd
    $ASROOT expect <<EOF
	spawn $DSCL -passwd /Users/$user
	expect "New Password:"
	send -- "$passwd\n"
	expect eof
EOF

    # We still need to LANMAN hash to work around case-sensitivity issues
    # with the NT hash.
    $ASROOT pwpolicy -u $user -p $passwd \
	-sethashtypes SMB-NT on SMB-LANMANAGER on

    $ASROOT $DSCL -passwd /Users/$user $passwd
    $ASROOT expect <<EOF
	spawn $DSCL -passwd /Users/$user
	expect "New Password:"
	send -- "$passwd\n"
	expect eof
EOF
}

ds_create_user()
{
    local user="$1"
    local passwd="$1"

    if ds_user_exists $user ; then
	true
    else
	uid=$(ds_find_next_uid)

	# GID 20 is "staff"
	$ASROOT $DSCL -create /Users/$user
	$ASROOT $DSCL -create /Users/$user  RealName Samba\ Test\ User
	$ASROOT $DSCL -create /Users/$user  UniqueID $uid
	$ASROOT $DSCL -create /Users/$user  PrimaryGroupID 20
	$ASROOT $DSCL -create /Users/$user  UserShell /bin/bash
	$ASROOT $DSCL -create /Users/$user  \
	    dsAttrTypeStandard:HomeDirectory /tmp/$user
	$ASROOT $DSCL -create /Users/$user  \
	    NFSHomeDirectory /tmp/$user

	#FIXME: this is (somehow) not the right createhomedir magic
	#$ASROOT createhomedir -c -l -u $user
	mkdir /tmp/$user

	ds_enable_user_for_smb "$user" "$passwd"
    fi
}

ds_count_user_groups()
{
    local user="$1"
    $DSCL -search /Groups GroupMembership $user | \
		    wc -l | awk '{print $1}'
}

ds_delete_user()
{
    local user="$1"

    # Remove the user from all groups. We don't pipe this straight into a
    # while loop, because that opens a race between two instances of dscl.
    # We don't use $DSSEARCH because we only want to modify the /Local domain.
    local grouplist=$($DSCL -search /Groups GroupMembership $user | \
		    awk '{print $1}')

    for group in $grouplist; do
	    $ASROOT $DSCL -delete /Groups/$group GroupMembership $user
    done

    $ASROOT $DSCL -delete /Users/$user
}

ds_lookup_user_name()
{
    uid="$1"

    $DSSEARCH -search /Users UniqueID $uid | awk '{print $1}' | head -1
}

ds_lookup_group_name()
{
    local gid="$1"

    # jpeach:~ jpeach$ dscl /Search -search /Groups PrimaryGroupID 501
    # local           PrimaryGroupID = (501)
    $DSSEARCH -search /Groups PrimaryGroupID $gid | awk '{print $1}' | head -1
}

ds_lookup_group_gid()
{
    group="$1"

    # jpeach:Headers jpeach$ dscl /Search -read /Groups/zip PrimaryGroupID
    # PrimaryGroupID: 25093
    gid=$($DSSEARCH -read /Groups/$group PrimaryGroupID | \
		awk '{print $2 ; exit }')
    if [ "$gid" = "" ]; then
	# For no reason that I can discern, /Search won't find /Local records,
	# so we need to retry. Gack.
	gid=$($DSCL -read /Groups/$group PrimaryGroupID | \
		    awk '{print $2 ; exit }')

    fi

    if [ "$gid" = "" ]; then
	false
    else
	echo $gid
    fi
}

ds_add_user_to_group()
{
    local user="$1"
    local gid="$2"

    local group=$(ds_lookup_group_name $gid)
    case $group in
	"") false ;;
	*) $ASROOT $DSCL -append /Groups/$group GroupMembership $user ;;
    esac
}

ds_create_group()
{
    local group="$1"
    $ASROOT $DSEDITGROUP -o create "$group"
}


ds_delete_group()
{
    local group="$1"
    $ASROOT $DSEDITGROUP -q -o delete "$group"
}

