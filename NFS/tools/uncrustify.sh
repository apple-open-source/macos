#!/bin/sh

#
# Check and optionally reformat source files using uncrustify.
# Usage: ./tools/uncrustify.sh
#

UNCRUSTIFY=`xcrun -find uncrustify`
CONF_FILE="./tools/xnu-uncrustify.cfg"
REPLACE="--replace"

declare -a dirs=(
	"mount_nfs"
	"ncctl"
	"nfs4mapid"
	"nfs_acl"
	"nfsclntTests"
	"nfsd"
	"nfsiod"
	"nfsstat"
	"rpc.lockd"
	"rpc.rquotad"
	"rpc.statd"
	"showmount"
	)

for dir in "${dirs[@]}"
do
	$UNCRUSTIFY -c $CONF_FILE --no-backup $REPLACE $dir/*.[chm]
done
