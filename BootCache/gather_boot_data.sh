#!/bin/bash

# Script to gather BootCache-related data for debugging purposes
#
# First, setup kernel tracing via the trace boot-arg, and optionally kperf:
#   sudo nvram boot-args='<existing boot-args> trace=0x3000000 kperf=p0x78f@10000000'
# Then, after rebooting, run this script to capture relevant files:
#   boot.tailspin - a ktrace of boot
#   warmd.spindump.txt - a spindump targetting warmd
#   oslogs.txt - logs from warmd/BootCache
#   statistics.txt - BootCache statistics
#   debugbuffer.txt - BootCache kext's logging buffer, if using a Development BootCache build (-configuration Development)
#   control.txt - BootCacheControl's log, if using a Development BootCache build (-configuration Development)
#   BootCache.playlist - BootCache playlist used for the current boot, if using a Development BootCache build (-configuration Development)
#   BootCache.history - BootCache recording from the current boot, if using a Development BootCache build (-configuration Development)
#   BootCache.omaphistory - BootCache recording from the current boot of omaps, if using a Development BootCache build (-configuration Development)
#   apfs.txt - volume/container layout for this machine
#   boot.spindump.txt - spindump of boot, based off the boot.tailspin
#   boot.fsusage.txt - fs_usage of boot, based off the boot.tailspin
#
#   Running fs_usage_dup_blocks_apfs against the fs_usage is also frequently useful:
#   ./fs_usage_dup_blocks_apfs boot.fsusage.txt > dupblocks.txt

date=$(date +%Y%m%dT%H%M%S)

if [ $# -ne 0 ] || [ ! -d $1 ]; then
	echo "Usage: $0 [directory]"
	exit 1
fi

if [ $# -eq 0 ] ; then
	basedir=/var/tmp
else
	basedir=$1
fi

dir=$basedir/boot-$date

echo "Saving BootCache data to $dir ..."

mkdir $dir

sudo sh -c "
sudo tailspin save --foreground -o -s $dir/boot.tailspin &
sudo spindump warmd 1 -nobs -o $dir/warmd.spindump.txt 2>&1 > $dir/spindump.log &
log show --last 60m --predicate 'eventMessage contains \"BootCache\" or sender contains \"BootCache\" or sender contains \"warmd\"' --info --debug --source > $dir/oslogs.txt &
sudo BootCacheControl statistics > $dir/statistics.txt
sudo BootCacheControl debugbuffer > $dir/debugbuffer.txt
sudo mv /var/log/BootCacheControl.log $dir/control.txt
sudo cp /var/tmp/BootCache* $dir/
sudo diskutil apfs list > $dir/apfs.txt

wait

sudo ktrace reset

echo \"Parsing boot.tailspin...\n\"

sudo spindump -i $dir/boot.tailspin -timeline -nobs -symbols $dir/warmd.spindump.txt -o $dir/boot.spindump.txt &
sudo fs_usage -wbR $dir/boot.tailspin > $dir/boot.fsusage.txt &

wait

"

echo "Done"
