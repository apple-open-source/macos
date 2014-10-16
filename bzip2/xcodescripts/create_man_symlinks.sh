#!/bin/sh

set -ex

DSTROOT=${DSTROOT}${INSTALL_PATH_PREFIX}

ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bunzip2.1
ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bzcat.1
ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bzip2recover.1

ln -s bzdiff.1 ${DSTROOT}/usr/share/man/man1/bzcmp.1

ln -s bzmore.1 ${DSTROOT}/usr/share/man/man1/bzless.1

