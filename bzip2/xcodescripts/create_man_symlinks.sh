#!/bin/sh

set -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	[ -d "${DSTROOT}/usr" ] && rm -rf "${DSTROOT}/usr"
	exit 0
fi

ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bunzip2.1
ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bzcat.1
ln -s bzip2.1 ${DSTROOT}/usr/share/man/man1/bzip2recover.1

ln -s bzdiff.1 ${DSTROOT}/usr/share/man/man1/bzcmp.1

ln -s bzmore.1 ${DSTROOT}/usr/share/man/man1/bzless.1

