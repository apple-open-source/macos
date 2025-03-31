#!/bin/sh -e

: ${rsync=/usr/bin/rsync}
: ${logfile="$PWD"/rsync.log}

:> "$logfile"

rm -rf dst
mkdir -p src dst

seq 1 4294967 > src/file
echo ">> FIRST TRANSFER" | tee "$logfile" 1>&2
# Tests that the sender can map the file and transfer it
$rsync --rsync-path="$rsync" --no-W -vv src/file dst 2>> "$logfile" 1>&2

truncate -s 16M dst/file
echo ">> SECOND TRANSFER" | tee "$logfile" 1>&2
# Tests that the receiver can map the file and transfer it
$rsync --rsync-path="$rsync" --no-W -vv src/file dst 2>> "$logfile" 1>&2
