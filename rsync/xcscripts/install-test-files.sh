#!/bin/sh
set -e -x

install -d -o root -g wheel -m 0755 "$DSTROOT"/AppleInternal/Tests/rsync
install -o root -g wheel -m 0755 "$SRCROOT"/tests/rsync.sh \
	"$DSTROOT"/AppleInternal/Tests/rsync

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
install -o root -g wheel -m 0644 "$SRCROOT"/tests/rsync.plist \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests

plutil -lint "$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/rsync.plist
