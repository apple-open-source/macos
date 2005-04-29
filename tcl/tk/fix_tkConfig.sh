#!/bin/sh

BOGUS_SRCROOT=${SRCROOT//tcl/tk}
BOGUS_OBJROOT=${OBJROOT//tcl/tk}
TK_CONFIG_SH=${DSTROOT}/System/Library/Frameworks/Tk.framework/Versions/8.4/tkConfig.sh

sed -e "s|${BOGUS_SRCROOT}|${SRCROOT}|" \
	-e "s|${BOGUS_OBJROOT}|${SYMROOT}|" \
	${TK_CONFIG_SH} > ${SYMROOT}/tkConfig.sh
cp ${SYMROOT}/tkConfig.sh ${TK_CONFIG_SH}
