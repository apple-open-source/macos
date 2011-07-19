#!/bin/sh

set -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	[ -d "${DSTROOT}/usr" ] && rm -rf "${DSTROOT}/usr"
	exit 0
fi

# This script phase is run here, so that the hardlinks are created *after* stripping.
# Doing it in the bzip2 target itself produces verification failures.
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bunzip2
ln ${DSTROOT}/usr/bin/bzip2 ${DSTROOT}/usr/bin/bzcat

