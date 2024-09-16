#!/bin/sh

# This file is assumed executed at the end of each build phase of IPConfiguration.framework.

TARGET_FRAMEWORK_NAME="IPConfiguration"
DYLIB_EXECUTABLE_PREFIX="lib"
DYLIB_PRODUCT_NAME="ipconfig"
DYLIB_EXECUTABLE_SYMLINK_BASE_NAME="${DYLIB_EXECUTABLE_PREFIX}${DYLIB_PRODUCT_NAME}" # expected 'libipconfig'

if [ "${ACTION}" != "installhdrs" ]; then
    # creates and goes to the dir where the original libipconfig.dylib resided, i.e. /usr/lib/
    DYLIB_INSTALL_DIR="/usr/lib"
    DYLIB_EXECUTABLE_SYMLINK_FULL_PATH="${DSTROOT}${DYLIB_INSTALL_DIR}" # expected path like '[...]/usr/lib'
    FRAMEWORK_EXECUTABLE_BASE_PATH_RELATIVE="../..${SYSTEM_LIBRARY_DIR:-/System/Library}/PrivateFrameworks"
    if [ ! -d ${DYLIB_EXECUTABLE_SYMLINK_FULL_PATH} ]; then
	mkdir -p ${DYLIB_EXECUTABLE_SYMLINK_FULL_PATH}
    fi
    cd ${DYLIB_EXECUTABLE_SYMLINK_FULL_PATH}
fi
    
#############################################
############# MACH-O EXECUTABLES ############
#############################################

# This points the dylib mach-o executable to the framework mach-o executable.
# The system shared cache will pick up this symlink and do the right thing.
# Only need the dylib symlink in the case of building for a live system
# with a shared cache (aka xbs buildit install phase).
if [ "${ACTION}" == "install" ]; then
    DYLIB_EXECUTABLE_SYMLINK_FULL_NAME="${DYLIB_EXECUTABLE_SYMLINK_BASE_NAME}.dylib"
    FRAMEWORK_EXECUTABLE_FULL_PATH_RELATIVE="${FRAMEWORK_EXECUTABLE_BASE_PATH_RELATIVE}/${TARGET_FRAMEWORK_NAME}.framework"
    # /usr/lib/libipconfig.dylib -> IPConfiguration
    ln -s "${FRAMEWORK_EXECUTABLE_FULL_PATH_RELATIVE}/${TARGET_FRAMEWORK_NAME}" ${DYLIB_EXECUTABLE_SYMLINK_FULL_NAME}
fi

#############################################
################# TBD FILES #################
#############################################

# This runs conditionally on the "installapi" and "install" build phases,
# but not on the "installhdrs" phase, and it correctly points the
# dylib's TBD file to the new framework's TBD file.
if [ "${ACTION}" != "installhdrs" ]; then
    # pwd is still $DSTROOT/usr/lib from above
    DYLIB_TBD_SYMLINK_BASE_NAME=${DYLIB_EXECUTABLE_SYMLINK_BASE_NAME}
    DYLIB_TBD_SYMLINK_FULL_NAME="${DYLIB_TBD_SYMLINK_BASE_NAME}.tbd"
    if [ -e ${DYLIB_TBD_SYMLINK_FULL_NAME} ]; then
	rm -f ${DYLIB_TBD_SYMLINK_FULL_NAME}
    fi
    # libipconfig.tbd -> IPConfiguration.tbd
    ln -s "${FRAMEWORK_EXECUTABLE_BASE_PATH_RELATIVE}/${TARGET_FRAMEWORK_NAME}.framework/${TARGET_FRAMEWORK_NAME}.tbd" ${DYLIB_TBD_SYMLINK_FULL_NAME}
fi

#############################################
################## HEADERS ##################
#############################################

DYLIB_HEADERS_BASE_PATH="/usr/local/include"
DYLIB_HEADERS_FULL_PATH="${DSTROOT}${DYLIB_HEADERS_BASE_PATH}"
FRAMEWORK_HEADERS_FULL_PATH_RELATIVE="../../..${SYSTEM_LIBRARY_DIR:-/System/Library}/PrivateFrameworks"

# goes to the dir where the original headers resided
# i.e. /usr/local/include in the SDKContentRoot
if [ ! -d ${DYLIB_HEADERS_FULL_PATH} ]; then
	mkdir -p ${DYLIB_HEADERS_FULL_PATH}
fi
cd ${DYLIB_HEADERS_FULL_PATH}

header_files=('DHCPv6PDService.h' 'IPConfigurationService.h' 'IPConfigurationUtil.h')
for header in ${header_files[@]}; do
    if [ -e ${header} ]; then
	rm -f ${header}
    fi
    # this needs to run at every buildit phase, so no need for conditionalizing
    ln -s "${FRAMEWORK_HEADERS_FULL_PATH_RELATIVE}/${TARGET_FRAMEWORK_NAME}.framework/PrivateHeaders/${header}" ${header}
done
