#!/bin/sh
set -e -x

# XXX We now have two copies of regress.m4 (shell_cmds, text_cmds) and will
# likely have more.  It would be nice to be able to install an authoritative
# copy in, e.g., /AppleInternal/Tests/bsd_common -- just once.
install -d -m 0755 "$DSTROOT"/AppleInternal/Tests/text_cmds
install -m 0644 "$SRCROOT"/tests/regress.m4 \
	"$DSTROOT"/AppleInternal/Tests/text_cmds

install -d -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

# The plist will use TARGET_* conditionals to avoid atf-sh tests on !macOS
tmplist=$(mktemp -t text_cmds_test_plist)
trap 'rm "$tmplist"' EXIT
xcrun clang -x c -C -P -E -imacros TargetConditionals.h	\
	-Wno-invalid-pp-token				\
	"$SRCROOT"/tests/text_cmds.plist.in		\
	-o "$tmplist"
plutil -lint "$tmplist"
install -m 0644 "$tmplist" \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/text_cmds.plist
