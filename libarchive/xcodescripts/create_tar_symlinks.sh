#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0
[ "$ACTION" == "installapi" ] && exit 0

# check if we're building for the simulator
[ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] && exit 0

ln -s bsdtar ${DSTROOT}/usr/bin/tar
ln -s bsdtar.1 ${DSTROOT}/usr/share/man/man1/tar.1
