#! /bin/sh
. "${SRCROOT}/xcscripts/include.sh"
lics="/usr/local/OpenSourceLicenses"
vers="/usr/local/OpenSourceVersions"
xmkdir $(dst ${lics} ${vers})
xinstall $(src openssh/LICENCE) $(dst ${lics}/OpenSSH.txt)
xinstall $(src OpenSSH.plist) $(dst ${vers})
