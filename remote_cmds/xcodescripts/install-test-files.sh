#!/bin/sh
set -xue
install -d -m 0755 "$DSTROOT"/AppleInternal/Tests/remote_cmds
install -d -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
tmplist=$(mktemp -t remote_cmds_test_plist)
trap 'rm "$tmplist"' EXIT
xcrun clang -x c -C -P -E -imacros TargetConditionals.h	\
	-Wno-invalid-pp-token				\
	"$SRCROOT"/tests/remote_cmds.plist.in		\
	-o "$tmplist"
plutil -lint "$tmplist"
install -m 0644 "$tmplist" \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/remote_cmds.plist
