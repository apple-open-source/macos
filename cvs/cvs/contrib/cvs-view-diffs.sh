#!/bin/sh

##
# View diffs in FileMerge
# Must be run in a working copy.
##
# Wilfredo Sanchez Jr. | wsanchez@apple.com
# Copyright 1998 Apple Computer, Inc.
##

##
# Set up PATH
##

MyPath=/usr/bin:/bin;

if [ -z "${PATH}" ]; then
    export PATH=${MyPath};
else
    export PATH=${PATH}:${MyPath};
fi

##
# Usage
##

usage ()
{
    echo "usage: $(basename $0) [-r <rev1> [-r <rev2>]] [-a <rev3>] [files...]";
    echo "	Compares <rev1> and <rev2> using FileMerge with optional ancestor <rev3>.";
    echo "	<rev1> defaults to the version the current working copy is based on.";
    echo "	<rev2> defaults to the current working copy.";
    echo "	If <rev1> is the current working copy, then the merge";
    echo "	  directory is set to the current working directory.";
    exit 2;
}

abort ()
{
    local TmpDir1 TmpDir2 TmpDirA;

    echo -n "Abort: cleaning up...";

    if [ -n "${TmpDir1}" ]; then rm -rf "${TmpDir1}"; fi;
    if [ -n "${TmpDir2}" ]; then rm -rf "${TmpDir2}"; fi;
    if [ -n "${TmpDirA}" ]; then rm -rf "${TmpDirA}"; fi;

    echo "done.";

    exit 1;
}

##
# Catch signals
##

trap abort 1 2 15

##
# Handle command line
##

if [ -z "${TMPDIR}" ]; then TMPDIR=/tmp; fi;

  TmpDir1=${TMPDIR}/cvs-diff-$$-1;
  TmpDir2=${TMPDIR}/cvs-diff-$$-2;
  TmpDirA=${TMPDIR}/cvs-diff-$$-A;
Revision1="";
Revision2="";
 Ancestor="";

if ! args=$(getopt a:r: $*); then usage; fi;
set -- ${args};
for option; do
    case "$option" in
      -a)
	Ancestor="-r $2";
	shift;shift;
	;;
      -r)
	if   [ -z "${Revision1}" ]; then
	    Revision1="-r $2";
	elif [ -z "${Revision2}" ]; then
	    Revision2="-r $2";
	else
	    "Too many revisions specified.";
	    usage;
	fi;
	shift;shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

for file; do
    if [ ! -e ${file} ]; then
	echo "`basename $0`: ${file}: No such file or directory.";
	exit 1;
    fi;
done;

if [ ! -d "CVS" ]; then
    echo "There is no version here. Exiting.";
    exit 1;
fi;

##
# Do The Right Thing
##

echo -n "Figuring out which files changed...";
if [ -z "${Revision1}" ]; then
    DiffFiles=$(cvs -fnq update ${Revision1} ${Revision2} $* | egrep -v '^U ' | cut -d ' ' -f 2);
else
    DiffFiles=$(cvs -fq diff --brief ${Revision1} ${Revision2} $* | egrep '^RCS file: ' | cut -d ' ' -f 3);
fi;
echo " done";

for File in ${DiffFiles}; do

    ##
    # This algorithm may seem overly abmitious, but the problem is that
    #  the cvs diff command doesn't give us the file names we want. The
    #  output of diff prints the filename without a path (relative or
    #  otherwise) and the "RCS file:" output from CVS has what we want --
    #  almost. The problem is if you have &module in your modules file,
    #  CVS/Repository doesn't necessarily reflect the Repository path for
    #  each subdirectory in '.', and that stinks. So we pick the path apart
    #  until we find the longest matching filename.
    # In the case where we used update instead of diff, this doesn't do
    #  anything interesting.
    ##

    File=$(echo "$File" | sed 's|,v$||');
    File=$(echo "$File" | sed 's|^/||');

    LastFile="";

    while [ ! -f "${File}" ]; do
	if [ "${LastFile}" = "${File}" ]; then break; fi;
	LastFile=${File};
	File=$(echo "${File}" | sed 's|^[^/]*/||');
    done;

    # Couldn't find a match; give up.
    if [ "${LastFile}" = "${File}" ]; then continue; fi;

    ##
    # Get the orginial version and save it for later use.
    ##

    echo -n "Fetching ${File}...";

    # If no <rev1> specified, let's figure out the version ${File} is based on.
    if [ -z "${Revision1}" ]; then
	RevisionRight="-r $(grep /$(basename "${File}")/ $(dirname "${File}")/CVS/Entries | sed 's|^[^/]*/[^/]*/||' | sed 's|/.*$||')";
    else
	RevisionRight="${Revision1}";
    fi;

    # Get <rev1>.
    mkdir -p "${TmpDir1}"/$(dirname "${File}");
    cvs -fq update -kb ${RevisionRight} -p "${File}" > "${TmpDir1}/${File}";

    # Get <rev2>, if needed.
    if [ -n "${Revision2}" ]; then
	mkdir -p "${TmpDir2}"/$(dirname "${File}");
	cvs -fq update -kb ${Revision2} -p "${File}" > "${TmpDir2}/${File}";
    fi;

    # Get Ancestor, if needed
    if [ -n "${Ancestor}" ]; then
	mkdir -p "${TmpDirA}"/$(dirname "${File}");
	cvs -fq update -kb ${Ancestor} -p "${File}" > "${TmpDirA}/${File}";
    fi;

    echo " done";

done;

# It would be cool if I could tell opendiff to tell FileMerge to:
#  - Not open the "Open" window when launched
#  - Filter "Added/Deleted"
if [ -d "${TmpDir1}" ]; then
    AncestorArgs="";

    if [ -n "${Ancestor}" ]; then AncestorArgs="-ancestor ${TmpDirA}"; fi;

    if [ -d "${TmpDir2}" ]; then
	opendiff "${TmpDir1}" "${TmpDir2}" ${AncestorArgs};
    else
	opendiff "${TmpDir1}" . -merge .   ${AncestorArgs};
    fi;
else
    echo "No diffs.";
fi;

# Because opendiff returns before the user is done, there is no
# way to know when the user has finished viewing the diffs, so
# I can't clean up ${TmpDir1}, ${TmpDir2}, and ${TmpDirA}
#rm -rf ${TmpDir1} ${TmpDir2} ${TmpDirA};
