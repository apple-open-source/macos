#!/bin/sh
# $Id: run_tic.in,v 1.34 2012/10/06 19:00:51 tom Exp $
##############################################################################
# Copyright (c) 1998-2011,2012 Free Software Foundation, Inc.                #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
#
# Author: Thomas E. Dickey 1996-on
#
# This script is used to install terminfo.src using tic.  We use a script
# because the path checking is too awkward to do in a makefile.
#
# Assumes:
#	The leaf directory names (lib, tabset, terminfo)
#
echo '** Building terminfo database, please wait...'
#
# The script is designed to be run from the misc/Makefile as
#	make install.data

: ${suffix:=}
: ${DESTDIR:=}
: ${prefix:=/usr/local}
: ${exec_prefix:=${prefix}}
: ${bindir:=${exec_prefix}/bin}
: ${top_srcdir:=..}
: ${srcdir:=.}
: ${datarootdir:=/usr/share}
: ${datadir:=${datarootdir}}
: ${TIC_PATH:=/usr/bin/tic}
: ${ticdir:=/usr/share/terminfo}
: ${source:=${top_srcdir}/misc/terminfo.src}
: ${LN_S:="ln -s -f"}
: ${cross_compiling:=no}
: ${ext_funcs:=1}

test -z "${DESTDIR}" && DESTDIR=

# Allow tic to run either from the install-path, or from the build-directory.
# Do not do this if we appear to be cross-compiling.  In that case, we rely
# on the host's copy of tic to compile the terminfo database.
if test "x$cross_compiling" = "xno"
then
	if test -f ../progs/tic$suffix
	then
		case "$PATH" in
		\:*)
			PATH="../progs:../lib:${DESTDIR}$bindir$PATH"
			;;
		*)
			PATH="../progs:../lib:${DESTDIR}$bindir:$PATH"
			;;
		esac
		export PATH
		if test shared = shared
		then
			SHLIB="sh $srcdir/shlib"
			TIC_PATH="$SHLIB tic"
		else
			TIC_PATH="tic"
		fi
	elif test "$TIC_PATH" = unknown
	then
		echo '? no tic program found'
		exit 1
	fi
else
	# Cross-compiling, so don't set PATH or run shlib.
	SHLIB=
	# reset $suffix, since it applies to the target, not the build platform.
	suffix=
fi


# set another env var that doesn't get reset when `shlib' runs, so `shlib' uses
# the PATH we just set.
SHLIB_PATH=$PATH
export SHLIB_PATH

# set a variable to simplify environment update in shlib
SHLIB_HOST=darwin23.0.0
export SHLIB_HOST

# don't use user's TERMINFO variable
TERMINFO=${DESTDIR}$ticdir ; export TERMINFO
umask 022

# Construct the name of the old (obsolete) pathname, e.g., /usr/lib/terminfo.
TICDIR=`echo $TERMINFO | sed -e 's%/share/\([^/]*\)$%/lib/\1%'`

# Parent directory may not exist, which would confuse the install for hashed
# database.  Fix.
PARENT=`echo "$TERMINFO" | sed -e 's%/[^/]*$%%'`
if test -n "$PARENT"
then
	test -d $PARENT || mkdir -p $PARENT
fi

# Remove the old terminfo stuff; we don't care if it existed before, and it
# would generate a lot of confusing error messages if we tried to overwrite it.
# We explicitly remove its contents rather than the directory itself, in case
# the directory is actually a symbolic link.
( test -d "$TERMINFO" && cd $TERMINFO && rm -fr ? 2>/dev/null )

if test "$ext_funcs" = 1 ; then
cat <<EOF
Running $TIC_PATH to install $TERMINFO ...

	You may see messages regarding extended capabilities, e.g., AX.
	These are extended terminal capabilities which are compiled
	using
		tic -x
	If you have ncurses 4.2 applications, you should read the INSTALL
	document, and install the terminfo without the -x option.

EOF
if ( $TIC_PATH -x -s -o $TERMINFO $source )
then
	echo '** built new '$TERMINFO
else
	echo '? tic could not build '$TERMINFO
	exit 1
fi
else
cat <<EOF
Running $TIC_PATH to install $TERMINFO ...

	You may see messages regarding unknown capabilities, e.g., AX.
	These are extended terminal capabilities which may be compiled
	using
		tic -x
	If you have ncurses 4.2 applications, you should read the INSTALL
	document, and install the terminfo without the -x option.

EOF
if ( $TIC_PATH -s -o $TERMINFO $source )
then
	echo '** built new '$TERMINFO
else
	echo '? tic could not build '$TERMINFO
	exit 1
fi
fi
