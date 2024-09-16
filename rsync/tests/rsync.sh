#!/bin/sh

# Really basic smoke test, to make sure that the wrapper isn't interfering with
# normal rsync operations.

fails=0
basedir=$(pwd)

rsync --help > ${basedir}/rsync.help
if [ $? -ne 0 ]; then
	1>&2 echo "rsync --help failed"
	fails=$((fails + 1))
elif ! grep -q -- --rsync-path ${basedir}/rsync.help; then
	# Choice of --rsync-path is arbitrary, we just need a definitively rsync
	# specific option.
	1>&2 echo "rsync --help output incorrect"
	fails=$((fails + 1))
elif grep -q -- samba ${basedir}/rsync.help; then
	# Make sure /usr/bin/rsync is no longer defaulting to rsync.samba
	1>&2 echo "rsync --help indicates rsync.samba"
	fails=$((fails + 1))
fi

mkdir foo
cd foo

mkdir a
touch a/{x,y,z}

ls -lR > ${basedir}/hier

rsync -avz a b > ${basedir}/rsync.out
if [ $? -ne 0 ]; then
	1>&2 echo "rsync -avz failed"
	fails=$((fails + 1))
fi

(cd b && ls -lR) > ${basedir}/hier.new

if ! cmp -s ${basedir}/hier ${basedir}/hier.new; then
	1>&2 echo "rsync -avz resulted in a different structure"
	fails=$((fails + 1))
fi

# Confirm that invoking /usr/bin/rsync with a long flag known to belong to both
# backing applications no longer goes to rsync.samba as well.
rsync --address=foo > ${basedir}/rsync-2.out 2>&1
if grep -q -- samba ${basedir}/rsync-2.out; then
	1>&2 echo "rsync --address went to samba rsync"
	fails=$((fails + 1))
fi

# --exclude is temporarily routed to smb rsync, among other options; make sure
# that's still working.
rsync --itemize-changes > ${basedir}/rsync-3.out 2>&1
if ! grep -q -- samba ${basedir}/rsync-3.out; then
	1>&2 echo "rsync --itemize-changes went to non-samba rsync"
	fails=$((fails + 1))
fi

# Finally, try selecting the samba implementation.
env CHOSEN_RSYNC=rsync_samba rsync --address=fo > ${basedir}/rsync-4.out 2>&1
if ! grep -q -- samba ${basedir}/rsync-4.out; then
	1>&2 echo "rsync --address with env should have gone to smb rsync"
	fails=$((fails + 1))
fi

if [ "${fails}" -eq 0 ]; then
	echo "All tests passed"
else
	echo "${fails} tests failed"
fi

exit ${fails}
