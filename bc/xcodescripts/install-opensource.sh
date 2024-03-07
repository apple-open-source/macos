#!/bin/sh

set -eu

plutil -lint "$SRCROOT"/bc.plist

install -m 0755 -d "${DSTROOT}"/usr/local/OpenSourceVersions
install -m 0644 "${SRCROOT}"/bc.plist \
    "${DSTROOT}"/usr/local/OpenSourceVersions

install -m 0755 -d "${DSTROOT}"/usr/local/OpenSourceLicenses
install -m 0644 "${SRCROOT}"/bc/LICENSE.md \
    "${DSTROOT}"/usr/local/OpenSourceLicenses/bc.txt
