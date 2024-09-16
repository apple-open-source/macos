#!/bin/sh
set -e -x

install -d -o root -g wheel -m 0755 "$DSTROOT"/AppleInternal/Tests/rsync
install -o root -g wheel -m 0755 "$SRCROOT"/tests/rsync.sh \
	"$DSTROOT"/AppleInternal/Tests/rsync

tmpdir=$(mktemp -dt rsync_test_plist)
tmplist="$tmpdir/rsync.plist"
trap 'rm -rf $tmpdir' EXIT

# First generate rsync.interop.plist from the srcroot into our tmpdir
"$SRCROOT"/xcscripts/build-interop-plist.sh \
	"$SRCROOT"/tests/rsync.interop.plist.in \
	"$tmpdir"/rsync.interop.plist

# Then assemble the final plist, which may #include our interop plist on
# compatible platforms.
xcrun clang -x c -C -P -E -imacros TargetConditionals.h \
	-Wno-invalid-pp-token -I"$tmpdir" \
	"$SRCROOT"/tests/rsync.plist.in \
	-o "$tmplist"

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
install -o root -g wheel -m 0644 "$tmplist" \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

plutil -lint "$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/rsync.plist
