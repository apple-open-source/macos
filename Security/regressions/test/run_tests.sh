#!/bin/sh

# run_tests.sh
# Security
#
# Created by Fabrice Gautier on 8/26/10.
# Copyright 2010 Apple, Inc. All rights reserved.


# Run a command line tool on the sim or the device

CMD=SecurityTests.app/SecurityTests

if [ "${PLATFORM_NAME}" == "iphoneos" ]; then
    INSTALL_DIR=/tmp
    RSYNC_URL=rsync://root@localhost:10873/root${INSTALL_DIR}
#copy Security Framework and testrunner program to the device, to /tmp
#This will not override the existing security Framework on your device (in /System/Library/Framework)
    export RSYNC_PASSWORD=alpine
    echo "run_tests.sh:${LINENO}: note: Copying stuff to device"
    /usr/bin/rsync -cav ${CONFIGURATION_BUILD_DIR}/Security.framework ${RSYNC_URL}
    /usr/bin/rsync -cav ${CONFIGURATION_BUILD_DIR}/SecurityTests.app ${RSYNC_URL}
    echo "run_tests.sh:${LINENO}: note: Running the test"
    xcrun -sdk "$SDKROOT" PurpleExec --env "DYLD_FRAMEWORK_PATH=${INSTALL_DIR}" --cmd ${INSTALL_DIR}/${CMD}
else
    echo "run_tests.sh:${LINENO}: note: Running test on simulator (${BUILT_PRODUCTS_DIR}/${CMD})"
	export DYLD_ROOT_PATH="${SDKROOT}"
    export DYLD_LIBRARY_PATH="${BUILT_PRODUCTS_DIR}"
    export DYLD_FRAMEWORK_PATH="${BUILT_PRODUCTS_DIR}"
    ${BUILT_PRODUCTS_DIR}/${CMD}
fi


