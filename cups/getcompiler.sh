#!/bin/sh
#
# Get the best compiler to use for the local platform
#
# Usage:
#
#    getcompiler.sh cc		To get the C compiler
#    getcompiler.sh cxx		To get the C++ compiler
#

if test $# != 1; then
	echo "Usage: getcompiler.sh {cc|cxx}"
	exit 1
fi

case "$1" in
	cc)
		if test -x /usr/bin/clang; then
			echo /usr/bin/clang
		else
			echo /usr/bin/gcc
		fi
		;;
	cxx)
		if test -x /usr/bin/clang++; then
			echo /usr/bin/clang++
		else
			echo /usr/bin/g++
		fi
		;;
	*)
		echo "Usage: getcompiler.sh {cc|cxx}"
		exit 1
		;;
esac
