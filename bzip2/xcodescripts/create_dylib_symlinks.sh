#!/bin/sh

set -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
    DSTROOT="$DSTROOT$SDKROOT"
    install_name_tool -id /usr/lib/libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.1.0.dylib
fi

ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.dylib
ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.1.0.5.dylib

