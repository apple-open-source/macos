#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0
[ "$ACTION" == "installapi" ] && exit 0

chmod +x ${DSTROOT}/usr/bin/bzdiff
ln -s bzdiff ${DSTROOT}/usr/bin/bzcmp

chmod +x ${DSTROOT}/usr/bin/bzmore
ln -s bzmore ${DSTROOT}/usr/bin/bzless

