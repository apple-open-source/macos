#!/bin/sh
#
# Generate swigwarn.swg from swigwarn.h
#
# Based on the "Lib/swigwarn.swg" target in the original Makefile.in
#
echo "/* Automatically generated file containing all the swig warning codes.  */" > ${SCRIPT_OUTPUT_FILE_0}
echo "/* Do not modify this file by hand, change 'Source/Include/swigwarn.h'  */" >> ${SCRIPT_OUTPUT_FILE_0}
echo "/* and use the command 'make Lib/swigwarn.swg' instead.                 */" >> ${SCRIPT_OUTPUT_FILE_0}
echo >> ${SCRIPT_OUTPUT_FILE_0}; echo >> ${SCRIPT_OUTPUT_FILE_0}
awk '/#define WARN/{$1="%define"; $2="SWIG"$2; $3=sprintf("%d %%enddef", $3); print $0; next;}\
      /#/{next;} {print $0}' < ${SCRIPT_INPUT_FILE_0} >> ${SCRIPT_OUTPUT_FILE_0}
