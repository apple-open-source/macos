#!/bin/sh

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
    DSTROOT="$DSTROOT$SDKROOT"
    install_name_tool -id /usr/lib/libarchive.2.dylib ${DSTROOT}/usr/lib/libarchive.2.dylib
fi

set -ex
ln -s libarchive.2.dylib ${DSTROOT}/usr/lib/libarchive.dylib

