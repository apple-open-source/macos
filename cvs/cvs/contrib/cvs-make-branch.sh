#!/bin/sh

##
# Create a branch.
# Tags the base version with <branch_tag>-base for future reference,
#  then creates a branch named <branch_tag>. This program wouldn't be
#  useful, except CVS doesn't provide a better way to reference the base
#  version, which is useful for cvs-diff-branch, for example. This isn't
#  really necessary for merge operations if you are merging into the branch
#  from which the new branch is created, but it is if you need to be able
#  to merge into multiple branches.
# For example, if you create a release branch, and then create bug fix
#  branches from that branch (or the trunk) and want to merge fix fixes
#  into either or both the release branch and the trunk depending on your
#  change control policy, then you need this.
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
    echo "Usage: $(basename $0) [-d repository] [-m <module> [-b <base_version>]] [<options>] <branch_tag>";
    echo "	<repository>: CVS repository to branch from.";
    echo "	<module>: Name of CVS module. If not specified, use working copy in \".\".";
    echo "	<base_version>: Version to branch from. Default is main branch if -m is";
    echo "	  specified, else the current working version.";
    echo "	<branch_tag>: Tag to use for new branch.";
    echo "Options: -a automaticly checkout (don't ask)";
    echo "         -l don't recurse down directories";
    echo "         -n don't update/checkout (don't ask)";
    echo "         -v be verbose";
    exit 22;
}

# Ask yes or no question
boolean_ask ()
{
  local prompt;
  local reply;

  prompt=$1;

  while (true); do
      echo -n "${prompt} " ; read reply;

      case "${reply}" in
        y | yes | Y | YES | Yes )
          return 0;
          ;;
        n | no | N | NO | No )
          return 1;
          ;;
        *)
          echo -n "Huh? "
          ;;
      esac
  done
}

##
# Handle command line
##

     Project="$(basename "$(pwd)") (working copy)";
     Version="<current version>";
Project_args="";
Version_args="";
 Tag_Command="tag";
      q_Flag="-Q";
      l_Flag="";
   Automatic="NO";
    Noaction="NO";

if ! args=$(getopt alnvd:m:b: $*); then usage; fi;
set -- ${args};
for option; do
    case "${option}" in
      -d)
	CVSROOT="$2"; export CVSROOT;
	shift; shift;
	;;
      -m)
	     Project=$2;
	Project_args="${Project}";
	     Version="<main branch>";
         Tag_Command="rtag";
	shift;shift;
	;;
      -b)
	if [ -z "${Project}" ]; then usage; fi;
	     Version=$2;
	Version_args="-r ${Version}";
	shift;shift;
	;;
      -l)
        l_Flag="-l";
	shift;
	;;
      -v)
	q_Flag="";
	shift;
	;;
      -a)
	Automatic="YES";
	shift;
	;;
      -n)
	Noaction="YES";
	shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

Branch=$1; if [ $# != 0 ]; then shift; fi;

if [ $# != 0 ]; then usage; fi;

if [ -z "${Branch}" ]; then usage; fi;

if [ -n "${l_Flag}" ] && [ "${Tag_Command}" = "rtag" ]; then
    echo "-l and -m are mutually exclusive options";
    usage;
fi;

if [ -z "${Project_args}" ] && [ ! -d "CVS" ]; then
    echo "There is no version here. Exiting.";
    exit 2;
fi;

##
# Confirm values
##

echo "Prepared to make branch:";
echo "Project      : ${Project}";
echo "Branch tag   : ${Branch}";
echo "Base version : ${Version}";

if [ "${Automatic}" = "NO" ] &&
   ! boolean_ask "Shall I continue?"; then
    exit 1;
fi;

##
# Do The Right Thing
##

echo -n "Tagging ${Branch}-base at ${Version} in ${Project}...";

cvs ${q_Flag} "${Tag_Command}" ${l_Flag} ${Version_args} "${Branch}-base" ${Project_args};

echo "done.";

echo -n "Branching ${Branch} on ${Branch}-base in ${Project}...";

cvs ${q_Flag} "${Tag_Command}" ${l_Flag} -r "${Branch}-base" -b "${Branch}" ${Project_args};

echo "done.";

if [ "${Noaction}" = "NO" ]; then
    if [ -z "${Project_args}" ]; then

        # Update to branch

        echo -n "Updating to branch ${Branch} in ${Project}...";

        cvs ${q_Flag} update ${l_Flag} -r "${Branch}";

        echo "done.";

    else

        # Check out project

	if [ "${Automatic}" = "YES" ] ||
	   boolean_ask "Shall I check out ${Branch} in ${Project}?"; then

	    echo -n "Checking out branch ${Branch} in ${Project}...";

	    cvs ${q_Flag} checkout -r "${Branch}" ${Project_args};

	    echo "done.";

	fi;
    fi;
fi;

##
# Bye
##

exit 0;
