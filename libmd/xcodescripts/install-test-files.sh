#!/bin/sh
set -e -x

install -d -o root -g wheel -m 0755 "$DSTROOT"/AppleInternal/Tests/libmd
install -o root -g wheel -m 0755 "$SRCROOT"/libmd/tests/file_test.sh \
	"$DSTROOT"/AppleInternal/Tests/libmd
install -o root -g wheel -m 0755 "$SRCROOT"/libmd/tests/legacy_test.sh \
	"$DSTROOT"/AppleInternal/Tests/libmd

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
install -o root -g wheel -m 0644 "$SRCROOT"/tests/libmd.plist \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
