#!/bin/sh
set -ex

install -o daemon -d "${DSTROOT}/private/var/at"
install -m 644 "${SRCROOT}/cron.deny" "${DSTROOT}/private/var/at"

mkdir -p "${DSTROOT}/private/var/at/tabs"
mkdir -p "${DSTROOT}/private/var/at/tmp"
chmod 700 "${DSTROOT}/private/var/at/tabs" "${DSTROOT}/private/var/at/tmp"

mkdir -p "${DSTROOT}/usr/local/OpenSourceVersions"
install -m 0444 "${SRCROOT}/cron.plist" "${DSTROOT}/usr/local/OpenSourceVersions/cron.plist"
mkdir -p "${DSTROOT}/usr/local/OpenSourceLicenses"
install -m 0444 "${SRCROOT}/LICENSE" "${DSTROOT}/usr/local/OpenSourceLicenses/cron.txt"
