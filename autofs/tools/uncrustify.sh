#!/bin/sh

#
# Check and optionally reformat source files using uncrustify.
# Usage: ./tools/uncrustify.sh
#

UNCRUSTIFY=`xcrun -find uncrustify`
CONF_FILE="./tools/xnu-uncrustify.cfg"
REPLACE="--replace"

declare -a dirs=(
	"autofs"
	"autofsd"
	"automount"
	"automountd"
	"automountlib"
	"checktrigger"
	"dumpammap"
	"dumpfstab"
	"files"
	"headers"
	"mig"
	"mount_autofs"
	"mount_url"
	"od_user_homes"
	"smbremountserver"
	"triggers"
	"watch_for_automounts"
	)

for dir in "${dirs[@]}"
do
	$UNCRUSTIFY -c $CONF_FILE --no-backup $REPLACE $dir/*.[chm] $dir/*.[ch]pp 2>&1 | grep -v -e  "Failed to load"
done
