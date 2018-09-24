#!/bin/bash -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0

# This script gets run after creating the dylib and before creating the tbd file
# <rdar://problem/39067080> GenerateTAPI phase should appear in the build phases of a target
mkdir -p ${INSTALL_DIR}

if [[ ${ACTION} == "install" ]] ; then
    ln -s -f ${EXECUTABLE_NAME} ${INSTALL_DIR}/libarchive.dylib
fi

ln -s -f ${EXECUTABLE_NAME/dylib/tbd} ${INSTALL_DIR}/libarchive.tbd
