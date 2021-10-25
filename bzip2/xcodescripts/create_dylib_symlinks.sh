#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0

# This script gets run after creating the dylib and before creating the tbd file
# <rdar://problem/39067080> GenerateTAPI phase should appear in the build phases of a target
mkdir -p ${INSTALL_DIR}

if [[ ${ACTION} == "install" ]] ; then
    ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.dylib
    ln -fs libbz2.1.0.dylib ${DSTROOT}/usr/lib/libbz2.1.0.8.dylib
fi

ln -fs libbz2.1.0.tbd ${DSTROOT}/usr/lib/libbz2.tbd
ln -fs libbz2.1.0.tbd ${DSTROOT}/usr/lib/libbz2.1.0.8.tbd

