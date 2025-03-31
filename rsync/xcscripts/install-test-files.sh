#!/bin/sh
set -e

export LANG=C.UTF-8

testdir="$DSTROOT"/AppleInternal/Tests/rsync

install -d -o root -g wheel -m 0755 "$testdir"
install -d -o root -g wheel -m 0755 "$testdir"/openrsync

# Install the Apple-specific test files from tests/ first.
for testf in rsync.sh pwauth.sh setsid.sh syslog_trace.sh xfer.sh; do
	install -o root -g wheel -m 0755 "$SRCROOT"/tests/"$testf" \
	    "$testdir"
done

# Install the various helper scripts from the openrsync test suite.
for testscript in "$SRCROOT"/tests/openrsync/*.sh; do
	install -o root -g wheel -m 0755 "$testscript" \
	    "$testdir"/openrsync
done

# Install the actual test files from the openrsync test suite.
for testprog in "$SRCROOT"/tests/openrsync/test*.test; do
	install -o root -g wheel -m 0755 "$testprog" \
	    "$testdir"/openrsync
done

# Generate the test program from this set.
(cd "$testdir"/openrsync && sh "$SRCROOT"/tests/openrsync/generate-kyua)

# rsync.samba lives in /AppleInternal/Tests/rsync already, symlink
# rsync.openrsync into there to simplify our test logic.
ln -sf /usr/bin/rsync "$testdir"/rsync.openrsync

tmpdir=$(mktemp -dt rsync_test_plist)
tmplist="$tmpdir/rsync.plist"
trap 'rm -rf $tmpdir' EXIT

# First generate rsync.daemon.plist and rsync.interop.plist from the srcroot
# into our tmpdir
"$SRCROOT"/xcscripts/build-daemon-plist.sh \
	"$SRCROOT"/tests/rsync.daemon.plist.in \
	"$tmpdir"/rsync.daemon.plist
"$SRCROOT"/xcscripts/build-interop-plist.sh \
	"$SRCROOT"/tests/rsync.interop.plist.in \
	"$tmpdir"/rsync.interop.plist

# Then assemble the final plist, which may #include our interop plist on
# compatible platforms.
xcrun clang -x c -C -P -E -imacros TargetConditionals.h \
	-Wno-invalid-pp-token -I"$tmpdir" \
	"$SRCROOT"/tests/rsync.plist.in \
	-o "$tmplist"

if ! plutil -lint "$tmplist"; then
	1>&2 echo "Generated test plist failed to lint"
	exit 1
fi

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
install -o root -g wheel -m 0644 "$tmplist" \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

plutil -lint "$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/rsync.plist
