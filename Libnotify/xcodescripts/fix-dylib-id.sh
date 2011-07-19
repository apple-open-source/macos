#!/bin/bash -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
        if [ -d ${DSTROOT}${SDKROOT}/usr/lib/system ] ; then
                for lib in ${DSTROOT}${SDKROOT}/usr/lib/system/*.dylib ; do
                        install_name_tool -id "${lib#${DSTROOT}${SDKROOT}}" "${lib}"
                done
        fi
fi
