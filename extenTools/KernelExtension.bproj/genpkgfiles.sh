#!/bin/sh

# 
# Copyright (c) 1996 NeXT Software, Inc.  All rights reserved. 
# Copyright (c) 1997-2000 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#
# genpkgfiles.sh
# - generate a driver .info file and copy over any package scripts
#
# HISTORY
#
# 6-Aug-96	Dieter Siegmund (dieter@next)
#      Created.
#


[ "$1" = "" -o "$2" = "" -o "$3" = "" ] && {
    echo "Usage: `basename $0` <driver project name> <driver source root> <package output dir>"
    exit 1
}

DRIVER_PROJECT_NAME=$1
INPUT_ROOT=$2
OUTPUT_ROOT=$3
DRIVER_INFO_FILE=$INPUT_ROOT/DriverInfo
PROG_NAME=`basename $0`
DIR_NAME=`echo $0 | sed 's%\(.*\)/[^/]*$%\1%'`

INFO_FILE=${OUTPUT_ROOT}/${DRIVER_PROJECT_NAME}.info

[ -d "$INPUT_ROOT" ] || {
    echo "$PROG_NAME: driver input root directory '$INPUT_ROOT' does not exist"
    exit 1
}

[ -d "$OUTPUT_ROOT" ] || {
    echo "$PROG_NAME: driver output root directory '$OUTPUT_ROOT' does not exist"
    exit 1
}

# source in the common version retrieval code
if [ "$DIR_NAME" = "$0" ]
then
	. getvers.sh
else
	. ${DIR_NAME}/getvers.sh
fi

# create driver long names
DRIVER_LONG_NAMES=""
DRIVER_SHORT_NAMES=""
for i in ${INPUT_ROOT}/English.lproj/*.strings
do
    [ ! "$DRIVER_LONG_NAMES" = "" ] && {
	DRIVER_LONG_NAMES="${DRIVER_LONG_NAMES}, "
    }
    DRIVER_LONG_NAMES=${DRIVER_LONG_NAMES}`sed -n 's/.*"Long Name"[       ]*=[    ]*"\([^"]*\)";/\1/p' $i`
    [ ! "$DRIVER_SHORT_NAMES" = "" ] && {
	DRIVER_SHORT_NAMES="${DRIVER_SHORT_NAMES}, "
    }
    DRIVER_SHORT_NAMES=${DRIVER_SHORT_NAMES}`sed -n "s/\"${DRIVER_PROJECT_NAME}\"[       ]*=[    ]*\"\([^\"]*\)\";/\1/p" $i`
done

DATE=`date`

echo "$PROG_NAME: generating '$INFO_FILE'"
sed "s/<<OS_NAME>>/$OS_NAME/g; s/<<DRIVER_VERSION>>/$DRIVER_VERSION/g; s/<<OS_RELEASE>>/$OS_RELEASE/g; s/<<DRIVER_PROJECT_NAME>>/$DRIVER_PROJECT_NAME/g; s|<<DRIVER_LONG_NAMES>>|$DRIVER_LONG_NAMES|g; s|<<DRIVER_SHORT_NAMES>>|$DRIVER_SHORT_NAMES|g; s|<<DRIVER_NAME>>|$DRIVER_NAME|g; s|<<DATE>>|$DATE|g;" > $INFO_FILE <<EOT
#
# <<DRIVER_PROJECT_NAME>>.info:
# - auto-generated on <<DATE>> for <<OS_NAME>> <<OS_RELEASE>>
# - driver supports <<DRIVER_SHORT_NAMES>>
#

# These fields will be displayed in the Info View
Title		<<DRIVER_NAME>>
Version		<<OS_NAME>> Release <<OS_RELEASE>>, Driver Version <<DRIVER_VERSION>>
Description	This package contains a driver bundle supporting the following devices: <<DRIVER_LONG_NAMES>>.  After installing, use Configure.app to add and configure the device to work with <<OS_NAME>>.


# These fields are used for the installed and floppy locations
DefaultLocation		/
Relocatable		NO
Diskname		<<DRIVER_PROJECT_NAME>> #%d

# Other fields
Application		NO
LibrarySubdirectory	Standard
InstallOnly 		YES
DisableStop		YES
LongFilenames		YES
EOT

#
# install the package scripts into the package area
#
for i in pre_install post_install pre_delete post_delete
do
    scriptname=$DRIVER_PROJECT_NAME.$i
    if [ -f $INPUT_ROOT/$scriptname ] 
    then
	echo "$PROG_NAME: copying '$INPUT_ROOT/$scriptname' to '$OUTPUT_ROOT/$scriptname'"
	cp $INPUT_ROOT/$scriptname $OUTPUT_ROOT/$scriptname
	chmod 755 $OUTPUT_ROOT/$scriptname
    fi
done

exit 0
