#!/bin/sh
# For embedded only:
[ "${PLATFORM_NAME}" = "macosx" ] && exit 0
. "${SRCROOT}/xcscripts/include.sh"
xmkdir $(dst /private/var/db/ssh)            # for host keys
