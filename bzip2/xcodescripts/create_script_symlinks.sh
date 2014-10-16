#!/bin/sh

set -ex

DSTROOT=${DSTROOT}${INSTALL_PATH_PREFIX}

chmod +x ${DSTROOT}/usr/bin/bzdiff
ln -s bzdiff ${DSTROOT}/usr/bin/bzcmp

chmod +x ${DSTROOT}/usr/bin/bzmore
ln -s bzmore ${DSTROOT}/usr/bin/bzless

