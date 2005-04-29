#!/bin/sh
##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# vers_string PROGRAM [STAMPED_NAME]
#
# Output a string suitable for use as a version identifier
#

##
# Usage
##

program=$(basename $0);

usage ()
{
    echo "Usage: ${program} [<options>] <program> [<stamped_name>]";
    echo "	<program>: ???";
    echo "	<stamped_name>: ???";
    echo "Options: ???";
}

##
# Handle command line
##

  Date=$(date);
Format=''\''PROGRAM:'\''"${Program}"'\''  PROJECT:'\''"${Version}"'\''  DEVELOPER:'\''"${USER}"'\''  BUILT:'\''"${Date}"'\'''\''\\\\n';

if ! args=$(getopt cflBn $*); then usage; fi;
set -- ${args};
for option; do
    case "${option}" in
      -c)
        Format=''\''#include <sys/cdefs.h>
__IDSTRING(SGS_VERS,"@(#)PROGRAM:'\''"${Program}"'\''  PROJECT:'\''"${Version}"'\''  DEVELOPER:'\''"${USER}"'\''  BUILT:'\''"${Date}"'\''\\\\n");
__IDSTRING(VERS_NUM,"'\''${Revision}'\''");\\n'\''';
	shift;
	;;
      -f)
        Format='"${Program}"'\''-'\''"${Revision}"\\\\n';
	shift;
	;;
      -l)
        Format=''\''#include <sys/cdefs.h>
__IDSTRING(SGS_VERS,"@(#)LIBRARY:'\''"${Program}"'\''  PROJECT:'\''"${Version}"'\''  DEVELOPER:'\''"${USER}"'\''  BUILT:'\''"${Date}"'\''\\\\n");'\''\\\\n';
	shift;
	;;
      -B)
        date="NO DATE SET (-B used)";
	shift;
	;;
      -n)
        Format='"${Revision}"\\\\n';
	shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

Program=$1; if [ $# != 0 ]; then shift; fi;
Version=$1; if [ $# != 0 ]; then shift; fi;

if [ $# != 0 ]; then usage; fi;

if [ -z "${Program}" ]; then Program="Unknown"; fi;

if [ -n "${Version}" ]; then
    if ! Revision=$(expr "${Version}" : '.*-\(.*\)'); then
	echo "${program}: No hyphen in project root ${Version}" >&2
	exit 1;
    fi;
else
    CurrentDir=$(/bin/pwd);
       Version=$(basename "${CurrentDir}");
    while [ "${Version}" != "${CurrentDir}" ]; do
	if Revision=$(expr "${Version}" : '.*-\(.*\)'); then break; fi;
	CurrentDir=$(dirname  "${CurrentDir}");
	   Version=$(basename "${CurrentDir}");
    done;
    if [ "${Version}" = "${CurrentDir}" ]; then
	CurrentDir=$(/bin/pwd);
	echo "${program}: No hyphen in project root ${CurrentDir}" >&2
	echo "${program}: Could not determine version" >&2
	 Version="Unknown";
	Revision="";
    fi;
fi;

if [ -z "${USER}" ]; then USER=$(whoami); fi;

printf "$(eval printf "${Format}")";
