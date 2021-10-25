#!/bin/sh

#
#  print-tcpdump-git-branch.sh
#

PROGNAME="`basename ${0}`"
COMMAND=tcpdump
USAGE="# usage: ${PROGNAME} [-h] [-p <path-of-tcdump-command>] [-v]"
VERBOSE=0

while getopts hp:v OPTION; do
	case ${OPTION} in
		h) echo "${USAGE}"; exit 0;;

		p) COMMAND="${OPTARG}";;

		v) VERBOSE=1;;

		\?) echo "${USAGE}"; exit 0;;
	esac
done

TMPFILE=`mktemp /tmp/${PROGNAME}.XXXXXX` || exit 1

#
# Grab the word after "Apple version ":
# - An official build version is made of digits [0-9] and dots '.'
# - A development build has the branch name followed by the build date
#
VERSION=`${COMMAND} --version 2>&1 |head -n 1|sed s/".*Apple version "//|awk '{ print $1}'`

if [ ${VERBOSE} -ge 1 ]; then
	echo "base version of ${COMMAND}: ${VERSION}"
fi

echo "${VERSION}" > "${TMPFILE}"

# Add 'tcpdump-' prefix for official builds
grep -q "[^0-9.]" "${TMPFILE}" &> /dev/null
if [ $? -eq 1 ]; then
	VERSION=tcpdump-"${VERSION}"
fi

rm "${TMPFILE}"

echo "${VERSION}"
