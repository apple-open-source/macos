#!/bin/sh

##
# Submit a project from CVS to Release Control.
# Export the sources into ~/Library/cvs-submit.
# Run submitproject on the result; delete the exported tree.
##
# Copyright 1998-2002 Apple Computer, Inc.
##

##
# Set up PATH
##

MyPath=/usr/bin:/bin;

if [ -z "${PATH}" ]; then
    export PATH=${MyPath};
else
    export PATH=${PATH}:${MyPath};
fi;

##
# Usage
##

usage ()
{
    echo "Usage: $(basename $0) [-r <version>] <file> [<file> ...]";
    echo "	<version>: Tag for version to revert to.";
    echo "	<file>: File(s) to revert.";
    echo "      If <version> is not specified, the checked-out version is assumed."
    echo "      If <file> is a directory, it is recursed into."
    echo "      If <file> is not specified, '.' is used as the default."
    echo "Options: -q be quiet";
    exit 22;
}

##
# Handle command line
##

q_Flag="";

if ! args=$(getopt qr: $*); then usage; fi;
set -- ${args};
for option; do
    case "$option" in
      -r)
	Revision="-r $2"
	shift; shift;
	;;
      -q)
	q_Flag="-q";
	shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

Files=$*;

if [ -z "${Files}" ]; then usage; fi;

##
# Functions
##

RevertFile ()
{
  local file=$1;

  if [ -z "${file}" ]; then return 22; fi;

  local basename="$(basename ${file})";
  local  dirname="$(dirname  ${file})";

  if [ -z "${dirname}" ]; then dirname=.; fi;

  local entry="$(egrep -e '^[^/]*/'"${basename}"'/' "${dirname}/CVS/Entries")";

  local type="$(echo ${entry} | awk -F / '{print $1}')";

  if [ -z "${entry}" ]; then
    if [ -d "${file}" ]; then type="D"; fi;
  fi;

  ##
  # Handle directories.
  # Use entry to determine if directory, because wrappers may effect what CVS
  #  thinks is a directory.
  ##
  if [ "${type}" = "D" ]; then

    if [ -z "${q_Flag}" ]; then
      echo "Reverting directory \"${file}\"...";
    fi;

    if [ ! -d "${file}" ]; then
      echo "Error: CVS thinks ${file} is a directory and I disagree.";
      return 20;
    fi;

    if [ ! -f "${file}/CVS/Entries" ]; then
      echo "Error: Directory ${file} is not a CVS-controlled directory.";
      return 22;
    fi;

    for child in $(cat "${file}/CVS/Entries" | awk -F / '{print $2}'); do
      RevertFile "${file}/${child}";
    done;

    return 0;
  fi;

  ##
  # Handle files
  ##
  if [ -z "${entry}" ]; then
    echo "File ${file} is not a CVS-controlled file. (Skipping.)";
    return 22;
  fi;

  local version="$(echo ${entry} | awk -F / '{print $3}')";

  # If ${file} is new (has been cvs added but not committed, we'll "revert"
  #  it by cvs removing it, but not deleting it.
  # Addition of a directory is not revertable, since directories are not versioned.
  if [ "${version}" = "0" ]; then
    mv -f "${file}" "/tmp/cvs-revert_${basename}_$$";
    cvs remove "${file}";
    mv "/tmp/cvs-revert_${basename}_$$" "${file}";
    return 0;
  fi;

  echo -n "X ${file} ";
  cvs -q update -p ${Revision} "${file}" > "${file}";
  if [ $? = 0 ]; then echo ""; fi;
}

##
# Do The Right Thing
##

for file in $Files; do
    RevertFile $file;
done;
