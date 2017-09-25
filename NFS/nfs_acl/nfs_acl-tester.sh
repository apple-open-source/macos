#!/bin/ksh

#
# Copyright (c) 2017 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#

NFSACLTOOL=/usr/local/bin//nfs_acl_tool
DBG=
TNAME="NFSv4 ACL test"
COMMON_PERMS="delete,readattr,writeattr,readextattr,writeextattr,readsecurity,writesecurity,chown"
FILE_PERMS="read,write,execute,append"
DIR_PERMS="list,add_file,search,add_subdirectory,delete_child"
TAGS="file_inherit,directory_inherit"
TFILE=file_$$
TDIR=dir_$$
TOPTS="--quiet"
typeset -i status=0 limit=0 keep=0

function Usage
{
	echo "Usage: ${0##*/} [-h] [-k] [-v] [-s] [-l limit_of_failures] { {[-u user] [-g group] | -f file_of_acls} TEST_DIR | TEST_DIR [ACL ...]}"
	cat <<EOF
	-h:		This usage.
	-k:		Keep file and directories even if test passes.
	-v:		Set the verbose option to the nfs_acl_tool
	-s:		Use the set option to nfs_acl_tool. ACLs will not be merged with current ACLs.
	-l no_of_fails:	Give up after the specified number of failures. Default is 1.
	-u user:	Use user for the default user in ACLs supplied by this test. 
	-g group:	Use group for the default group in ACLs supplied by this test.
	-f file:	Do not use the ACLs supplied by this test, but rather obtain the ACLs one per line from file
	TEST_DIR	Path to directory to run the test in. Will be created if necessary.
	[ ACL ...]	ACLs on the command line overrides the default test.

ACLS are of the form of chmod(1) with the following modifications.
	Multiple ACES and be specified by separating then with semicolons and no intervening space
	If ACLs are proceded by "F: " (note following space) then apply the ACL only to a file.
	IF ACLs are proceded by "D: " (note following space) then apply the ACL only to a directory.
	Otherwise apply the ACL to both files and directories.

Note ACLs on the command line must be quoted.
EOF
	exit 1
}

while getopts "Dkf:l:vsu:g:h" opt
do
	case $opt in
	D) DBG=echo;;
	k) TOPTS="$TOPTS --keep"; keep=1;;
	l) limit=$(($OPTARG+0));;
	v) TOPTS="$TOPTS --verbose"
		if ((limit == 0)); then limit=5; fi;;
	s) TOPTS=$"TOPTS --set";;
	f) ACLFILE=$OPTARG;;
	u) ACLUSER=$OPTARG;;
	g) ACLGROUP=$OPTARG;;
	h|*) Usage;;
	esac
done
shift $(($OPTIND-1))

if (( $# < 1 )); then
	Usage
fi
ROOT_DIR=$1
shift

if [[ $ACLFILE != "" ]]; then
	if [[ ($# > 0 || $ACLUSER != "" || $ACLGROUP != "") ]]; then
		Usage
	fi
else
	if [[ $# > 0 && ( $ACLUSER != "" || $ACLGROUP != "") ]]; then
		Usage
	fi
fi

if [[ ! -d $ROOT_DIR ]]; then
	mkdir $ROOT_DIR || exit 1
	makedir=1
fi
cd $ROOT_DIR

touch $TFILE || exit 1
mkdir $TDIR || exit 1

function getinput
{
	typeset acl

	: ${ACLUSER:=smbtest}
	: ${ACLGROUP:=everyone}

	if [[ -n $ACLFILE ]]; then
		cat $ACLFILE
	elif [[ $# > 0 ]]; then
		for acl in "$@"; do echo $acl; done
	else
		cat <<EOF
	F: user:$ACLUSER allow $FILE_PERMS,$COMMON_PERMS
	D: user:$ACLUSER allow $DIR_PERMS,$COMMON_PERMS
	group:$ACLGROUP allow $FILE_PERMS,$DIR_PERMS,$COMMON_PERMS,$TAGS
	user:$ACLUSER deny writesecurity,delete_child,$TAGS
EOF
	fi
}

echo "[BEGIN] $TNAME"

getinput "$@" |
while read ftype owner atype perms 
do
	if [[ $perms == "" ]]; then
		perms=$atype
		atype=$owner
		owner=$ftype
		ftype="all"
	fi
	acl="$owner $atype $perms"

	case $ftype in
	F:)
		echo "[INFO] setting acl $acl on $TFILE"
		$DBG  $NFSACLTOOL $TOPTS -a "$acl" $TFILE || ((status++));;
	D:)
		echo "[INFO] seting acl $acl on $TDIR"
		$DBG $NFSACLTOOL $TOPTS -a "$acl" $TDIR || ((status++));;
	*)
		echo "[INFO] setting acl $acl on $TFILE"
		$DBG $NFSACLTOOL $TOPTS -a "$acl" $TFILE || ((status++))
		echo "[INFO] seting acl $acl on $TDIR"
		$DBG $NFSACLTOOL $TOPTS -a "$acl" $TDIR || ((status++));;
	esac

	if ((status > limit)); then
		break;
	fi
done

if [[ $status == 0 && $keep == 0 ]]; then
	echo "[INFO] cleaning up"
	rm -f $TFILE
	rm -rf $TDIR
	if [[ $makedir == 1 ]]; then
		rmdir $ROOT_DIR
	fi
fi

if [[ $status == 0 ]]; then
	echo "[PASS] $TNAME"
else
	echo "[FAIL] $TNAME"
fi

exit $status
