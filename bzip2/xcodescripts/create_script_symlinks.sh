#!/bin/sh

set -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	[ -d "${DSTROOT}/usr" ] && rm -rf "${DSTROOT}/usr"
	exit 0
fi

chmod +x ${DSTROOT}/usr/bin/bzdiff
ln -s bzdiff ${DSTROOT}/usr/bin/bzcmp

chmod +x ${DSTROOT}/usr/bin/bzgrep
ln -s bzgrep ${DSTROOT}/usr/bin/bzegrep
ln -s bzgrep ${DSTROOT}/usr/bin/bzfgrep

chmod +x ${DSTROOT}/usr/bin/bzmore
ln -s bzmore ${DSTROOT}/usr/bin/bzless

