#!/bin/bash -ex

# check if we're building for the simulator
if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	if [ -n ${DSTROOT} -a -d ${DSTROOT}/usr ] ; then
		rm -rf ${DSTROOT}/usr
	fi
fi
