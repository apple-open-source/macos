#!/bin/sh

# Copyright (c) 2021 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#
# @APPLE_LICENSE_HEADER_END@

fileNumber=0
while [ $fileNumber -lt $SCRIPT_OUTPUT_FILE_COUNT ]
do
    eval xcrun --sdk \$SDK_NAME unifdef \$COPY_HEADERS_UNIFDEF_FLAGS -o \"\$SCRIPT_OUTPUT_FILE_$fileNumber\" \"\$SCRIPT_INPUT_FILE_$fileNumber\"
    
    # unifdef returns 0 and 1 when successful (0 if the output file is the
    # same as the input file, 1 if they're different).
    returnValue=$?
    if [ \( $returnValue -ne 0 \) -a \( $returnValue -ne 1 \) ]
    then
        exit $returnValue
    fi
    
    fileNumber=$((fileNumber+1))
done
