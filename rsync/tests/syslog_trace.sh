#!/bin/sh

: ${rsync=rsync}

# TEST 1: Test that the syslog trace support doesn't clobber errno and hide
# errors from user when enabled.

rm -rf src dst
mkdir -p src dst

# Revoke write permission from our target directory to cause an error emission
# that should indicate EACCES.
chmod u-w dst

touch src/foo
$rsync --rsync-path="$rsync" src/foo dst 2> error.log

if ! grep -q 'Permission denied' error.log; then
	1>&2 echo "Error appears to have been clobbered!"
	1>&2 cat error.log
	exit 1
fi

# TEST 2: Test that we don't log anything if we caught a signal, because
# os_log(3) is not async-signal-safe.
chmod u+w dst

dd if=/dev/random of=src/foo bs=8m count=1
$rsync --rsync-path="$rsync" --bwlimit=1 src/foo dst &
rpid=$!

while ! ps "$rpid" >/dev/null; do
	sleep 0.1
done

# Sleep a little bit longer to give the rsync process time to setup any signal
# handlers, which should happen quite early on.
sleep 0.5

kill -INT "$rpid"
wait "$rpid"

if log show --process "$rpid" | grep -qvE 'Timestamp|Activity|Default'; then
	1>&2 echo "Signalled rsync($rpid) seems to have written to os_log(3)"
	exit 1
fi
