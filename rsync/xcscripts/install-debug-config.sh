#!/bin/sh

set -ex

if [ "$RC_TARGET_CONFIG" != "iPhone" ]; then
    exit 0
fi

plutil -replace ProgramArguments \
    -json '["/var/personalized_debug/usr/bin/rsync", "--daemon", "--config=/var/personalized_debug/private/etc/rsyncd.debug.conf"]' \
    -o "${DSTROOT}/System/Library/LaunchDaemons/rsync.debug.plist" \
    "${DSTROOT}/System/Library/LaunchDaemons/rsync.plist"
plutil -replace Label -string com.apple.rsyncd.debug "${DSTROOT}/System/Library/LaunchDaemons/rsync.debug.plist"

sed 's|\(secrets file = \)|\1/var/personalized_debug/private|' < "${DSTROOT}/private/etc/rsyncd.conf" > "${DSTROOT}/private/etc/rsyncd.debug.conf"
