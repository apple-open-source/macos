#!/bin/sh

set -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	[ -d "${DSTROOT}${SDKROOT}/usr/bin" ] && rm -rf "${DSTROOT}${SDKROOT}/usr/bin"
fi

exit 0
