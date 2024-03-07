#!/bin/sh

set -eu

plutil -lint "${SRCROOT}"/tests/bc.plist

install -m 0755 -d "${DSTROOT}"/AppleInternal/CoreOS/BATS/unit_tests
install -m 0644 "${SRCROOT}"/tests/bc.plist \
    "${DSTROOT}"/AppleInternal/CoreOS/BATS/unit_tests

install -m 0755 -d "${DSTROOT}"/AppleInternal/Tests/bc
rsync -rlp "${SRCROOT}"/bc/scripts "${SRCROOT}"/bc/tests \
    "${DSTROOT}"/AppleInternal/Tests/bc
