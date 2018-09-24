#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0
[ "$ACTION" == "installapi" ] && exit 0

mv ${DSTROOT}/usr/local/OpenSourceLicenses/COPYING ${DSTROOT}/usr/local/OpenSourceLicenses/libarchive.txt

