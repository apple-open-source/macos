#!/bin/sh
#ident $Id: next-cvs_project_version.sh,v 1.5 2004/10/26 08:44:50 davep Exp $

##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
##

# from /usr/bin/vers_string

test=test

# default vers file is ./CVSVersionInfo.txt
versFile=CVSVersionInfo.txt

cflag=0
fflag=0
lflag=0
Bflag=0
nflag=0

while $test $# -gt 0 ; do
	case $1 in
		-c) cflag=1;;
		-l) lflag=1;;
		-f) fflag=1;;
		-B) Bflag=1;;
		-n) nflag=1;;
		-V) versFile=$2;;
		*) argProjName=$1;;
	esac
	shift
done

#echo cflag $cflag
#echo fflag $fflag
#echo lflag $lflag
#echo Bflag $Bflag
#echo nflag $nflag

if $test '!' -r $versFile ; then
	# This used to call `$projectInfo -rootProjectDir -inDirectory $wd`
	# to try to get the vers file if it wasn't found in the working
	# directory, but the -rootProjectDir flag to projectInfo
	# is no longer supported.
	echo "error: could not find CVSVersionInfo.txt for project versioning" 1>&2
	exit 1
fi


isWinNT=`arch | grep winnt`

# on OpenStep For Windows we have to use gawk
case $isWinNT in
	*winnt*) awk=gawk;;
	*) awk=awk;;
esac


user=`$awk '/\\$\Id:/ {print $7;}' $versFile`
timestamp=`$awk '/\\$\Id:/ {print $5 " " $6;}' $versFile`
fileVers=`$awk '/\\$\Id:/ {print $4;}' $versFile`
name=`$awk '/^ProjectName:/ {print $2;}' $versFile`
tag=`$awk '/\\$\Name:/ {print $3;}' $versFile`
versNum=`$awk '/^ProjectVersion:/ {print $2;}' $versFile`


PROJ=${argProjName-$name}
clean_proj_name=`echo $PROJ | sed 's/[^0-9A-Za-z]/_/g'`


#debugging 
#echo user $user 
#echo timestamp $timestamp 
#echo fileVers $fileVers
#echo name $name
#echo PROJ $PROJ
#echo clean_proj_name $clean_proj_name
#echo tag $tag
#echo versNum $versNum


# the keyword name is the current tag and only gets filled in if 
# the files are extracted at that tag. If the tag has a value,
# then it is a released version, if the tag is '$' then
# it has no value and is a development version.
#
case $tag in
	'$') vers="$versNum.dev";;
	*)   vers=$versNum;;
esac

q='"'

if $test $fflag = 1 ; then
	echo $PROJ-$vers
elif $test $nflag = 1 ; then
	echo $versNum
else
	vers_str="Project:$PROJ Version:$vers (Checkpoint by:$user on:$timestamp revision:$fileVers)"

	if $test $lflag = 1 ; then	
		echo "const char "$clean_proj_name"_VERS_STRING[200] = $q@(#)$vers_str$q;"
	elif $test $cflag = 1 ; then
		echo "const char "$clean_proj_name"_VERS_STRING[200] = $q@(#)$vers_str$q;"
		echo "const char "$clean_proj_name"_VERS_NUM[32] = $q$vers$q;"
	else
		echo $vers_str
	fi
fi

exit 0
