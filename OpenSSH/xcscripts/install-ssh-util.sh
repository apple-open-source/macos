#!/bin/sh
# Install on macOS only:
[ "${PLATFORM_NAME}" = "macosx" ] || exit 0
. "${SRCROOT}/xcscripts/include.sh"
xmkdir $(dst /usr/local/bin)
INSTALL_MODE=0755 xinstall $(src ssh-util.rb) $(dst /usr/local/bin/ssh-util)
