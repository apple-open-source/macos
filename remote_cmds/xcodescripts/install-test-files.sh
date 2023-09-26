#!/bin/sh
set -xue
install -d -o root -g wheel -m 0755 "$DSTROOT"/AppleInternal/Tests/remote_cmds
install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
xcrun clang -x c -C -P -E -imacros TargetConditionals.h	\
	-Wno-invalid-pp-token				\
	"$SRCROOT"/tests/remote_cmds.plist.in		\
	-o "$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/remote_cmds.plist
