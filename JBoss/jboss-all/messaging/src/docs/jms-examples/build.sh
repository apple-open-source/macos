#!/bin/sh
### ====================================================================== ###
##                                                                          ##
##  Copyright (c) 1998-2000 by Jason Dillon <jason@planet57.com>            ##
##                                                                          ##
##  This file is part of Buildmagic.                                        ##
##                                                                          ##
##  This library is free software; you can redistribute it and/or modify    ##
##  it under the terms of the GNU Lesser General Public License as          ##
##  published by the Free Software Foundation; either version 2 of the      ##
##  License, or (at your option) any later version.                         ##
##                                                                          ##
##  This library is distributed in the hope that it will be useful, but     ##
##  WITHOUT ANY WARRANTY; without even the implied warranty of              ##
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       ##
##  Lesser General Public License for more details.                         ##
##                                                                          ##
### ====================================================================== ###
##                                                                          ##
##  This is the main entry point for the build system.                      ##
##  Users should be sure to execute this file rather than 'ant' to ensure   ##
##  the correct version is being used with the correct configuration.       ##
##                                                                          ##
### ====================================================================== ###

# $Id: build.sh,v 1.1 2001/09/13 02:39:50 chirino Exp $

PROGNAME=`basename $0`
DIRNAME=`dirname $0`
GREP="grep"
ROOT="/"

# the default search path for buildmagic/ant
ANT_SEARCH_PATH="\
    tools/planet57/buildmagic \
    tools/buildmagic \
    buildmagic \
    tools/apache/ant \
    tools/ant \
    ant"

# the default build file name
ANT_BUILD_FILE="build.xml"

# the default arguments
ANT_OPTIONS="-find $ANT_BUILD_FILE"

# the required version of Ant
ANT_VERSION="1.3"

#
# Helper to complain.
#
die() {
    echo "${PROGNAME}: $*"
    exit 1
}

#
# Helper to source a file if it exists.
#
maybe_source() {
    for file in $*; do
	if [ -f "$file" ]; then
	    . $file
	fi
    done
}

search() {
    search="$*"
    for d in $search; do
	ANT_HOME="`pwd`/$d"
	ANT="$ANT_HOME/bin/ant"
	if [ -x "$ANT" ]; then
	    # found one
	    echo $ANT
	    break
	fi
    done
}

#
# Main function.
#
main() {
    # if there is a build config file. then source it
    maybe_source "$DIRNAME/build.conf" "$HOME/.build.conf"

    # try our best to find ANT
    if [ "x$ANT" = "x" ]; then
	found=""
	
	if [ "x$ANT_HOME" != "x" ]; then
	    ANT="$ANT_HOME/bin/ant"
	    if [ -x "$ANT" ]; then
		found="true"
	    fi
	else
	    # try the search path
	    ANT=`search $ANT_SEARCH_PATH`
	    target="build"
	    _cwd=`pwd`

	    while [ "x$ANT" = "x" ] && [ "$cwd" != "$ROOT" ]; do
		cd ..
		cwd=`pwd`
		ANT=`search $ANT_SEARCH_PATH`
	    done

	    # make sure we get back
	    cd $_cwd

	    if [ "$cwd" != "$ROOT" ]; then
		found="true"
	    fi
	fi

	# complain if we did not find anything
	if [ "$found" != "true" ]; then
	    die "Could not locate Ant; check \$ANT or \$ANT_HOME."
	fi
    fi

    # make sure we have one
    if [ ! -x "$ANT" ]; then
	die "Ant file is not executable: $ANT"
    fi

    # perhaps check the version
    if [ "x$ANT_VERSION_CHECK" != "x" ]; then
	result="`$ANT -version 2>&1 | $GREP $ANT_VERSION`x"
	if [ "$result" = "x" ]; then
	    die "Ant version $ANT_VERSION is required to build."
	fi
    fi

    export ANT ANT_HOME
    exec $ANT $ANT_OPTIONS "$@"
}

##
## Bootstrap
##

main "$@"
