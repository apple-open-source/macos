#!/bin/sh
### ====================================================================== ###
##                                                                          ##
##  This is the main entry point for the build system.                      ##
##                                                                          ##
##  Users should be sure to execute this file rather than 'ant' to ensure   ##
##  the correct version is being used with the correct configuration.       ##
##                                                                          ##
### ====================================================================== ###

# $Id$

PROGNAME=`basename $0`
DIRNAME=`dirname $0`
GREP="grep"
ROOT="/"

# Ignore user's ANT_HOME if it is set
ANT_HOME=""

# the default search path for ant
ANT_SEARCH_PATH="\
    tools
    tools/ant \
    tools/apache/ant \
    ant"

# the default build file name
ANT_BUILD_FILE="build.xml"

# the default arguments
ANT_OPTIONS="-find $ANT_BUILD_FILE"

# Use the maximum available, or set MAX_FD != -1 to use that
MAX_FD="maximum"

# OS specific support (must be 'true' or 'false').
cygwin=false;
darwin=false;
case "`uname`" in
    CYGWIN*)
        cygwin=true
        ;;

    Darwin*)
        darwin=true
        ;;
esac

# the jaxp parser to use
if [ "x$JAXP" = "x" ]; then
    # Default to crimson
    JAXP="crimson"
fi

#
# Helper to complain.
#
die() {
    echo "${PROGNAME}: $*"
    exit 1
}

#
# Helper to complain.
#
warn() {
    echo "${PROGNAME}: $*"
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
	    echo $ANT_HOME
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

    # Increase the maximum file descriptors if we can
    if [ $cygwin = "false" ]; then
	MAX_FD_LIMIT=`ulimit -H -n`
	if [ $? -eq 0 ]; then
	    if [ "$MAX_FD" = "maximum" -o "$MAX_FD" = "max" ]; then
		# use the system max
		MAX_FD="$MAX_FD_LIMIT"
	    fi

	    ulimit -n $MAX_FD
	    if [ $? -ne 0 ]; then
		warn "Could not set maximum file descriptor limit: $MAX_FD"
	    fi
	else
	    warn "Could not query system maximum file descriptor limit: $MAX_FD_LIMIT"
	fi
    fi

    # try the search path
    ANT_HOME=`search $ANT_SEARCH_PATH`

    # try looking up to root
    if [ "x$ANT_HOME" = "x" ]; then
	target="build"
	_cwd=`pwd`

	while [ "x$ANT_HOME" = "x" ] && [ "$cwd" != "$ROOT" ]; do
	    cd ..
	    cwd=`pwd`
	    ANT_HOME=`search $ANT_SEARCH_PATH`
	done

	# make sure we get back
	cd $_cwd

	if [ "$cwd" != "$ROOT" ]; then
	    found="true"
	fi

	# complain if we did not find anything
	if [ "$found" != "true" ]; then
	    die "Could not locate Ant; check \$ANT or \$ANT_HOME."
	fi
    fi

    # make sure we have one
    ANT=$ANT_HOME/bin/ant
    if [ ! -x "$ANT" ]; then
	die "Ant file is not executable: $ANT"
    fi

    # specify the jaxp parser impls to use
    case "$JAXP" in
	crimson)
	    JAXP_DOM_FACTORY="org.apache.crimson.jaxp.DocumentBuilderFactoryImpl"
	    JAXP_SAX_FACTORY="org.apache.crimson.jaxp.SAXParserFactoryImpl"
	    ;;
	   
	xerces)
	    JAXP_DOM_FACTORY="org.apache.xerces.jaxp.DocumentBuilderFactoryImpl"
	    JAXP_SAX_FACTORY="org.apache.xerces.jaxp.SAXParserFactoryImpl"
	    ;;
    esac

    if [ "x$JAXP_DOM_FACTORY" != "x" ]; then
	ANT_OPTS="$ANT_OPTS -Djavax.xml.parsers.DocumentBuilderFactory=$JAXP_DOM_FACTORY"
    fi
    if [ "x$JAXP_SAX_FACTORY" != "x" ]; then
	ANT_OPTS="$ANT_OPTS -Djavax.xml.parsers.SAXParserFactory=$JAXP_SAX_FACTORY"
    fi

    # need to specify planet57/buildmagic protocol handler package
    ANT_OPTS="$ANT_OPTS -Djava.protocol.handler.pkgs=org.jboss.net.protocol"

    # setup some build properties
    ANT_OPTS="$ANT_OPTS -Dbuild.script=$0"

    # change to the directory where the script lives so users are not forced
    # to be in the same directory as build.xml
    cd $DIRNAME

    # export some stuff for ant
    export ANT ANT_HOME ANT_OPTS

    # execute in debug mode, or simply execute
    if [ "x$ANT_DEBUG" != "x" ]; then
	/bin/sh -x $ANT $ANT_OPTIONS "$@"
    else
	exec $ANT $ANT_OPTIONS "$@"
    fi
}

##
## Bootstrap
##

main "$@"
