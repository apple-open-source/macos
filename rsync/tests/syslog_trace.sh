#!/bin/sh

: ${rsync=rsync}

# Test that the syslog trace support doesn't clobber errno and hide errors from
# user when enabled.

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
