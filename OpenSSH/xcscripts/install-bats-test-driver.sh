#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"
dir="$(dst /AppleInternal/CoreOS/BATS/unit_tests)"
xmkdir ${dir}
xinstall $(src BATS.plist) ${dir}/OpenSSH.plist
