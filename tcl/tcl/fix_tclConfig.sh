#!/bin/sh

TCL_CONFIG_SH=${DSTROOT}/System/Library/Frameworks/Tcl.framework/Versions/8.4/tclConfig.sh
sed -e "s|-arch [^ ']*||g" ${TCL_CONFIG_SH} > ${SYMROOT}/tclConfig.sh
cp ${SYMROOT}/tclConfig.sh ${TCL_CONFIG_SH}
