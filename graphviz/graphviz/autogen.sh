#!/bin/sh
# Run this to generate all the initial makefiles, etc.

# default tools
AUTOMAKE=automake
ACLOCAL=aclocal
AUTOCONF=autoconf
LIBTOOL=libtool
LIBTOOLIZE=libtoolize

# prefer known working versions if available
if test -x /usr/bin/automake-1.5 ; then AUTOMAKE=automake-1.5 ; fi 
if test -x /usr/bin/automake-1.6 ; then AUTOMAKE=automake-1.6 ; fi
if test -x /usr/bin/aclocal-1.5 ; then ACLOCAL=aclocal-1.5 ; fi
if test -x /usr/bin/aclocal-1.6 ; then ACLOCAL=aclocal-1.6 ; fi
if test -x /usr/bin/autoconf-2.53 ; then AUTOCONF=autoconf-2.53 ; fi
if test -x /usr/bin/autoconf-2.57 ; then AUTOCONF=autoconf-2.57 ; fi

echo "Using tools: $AUTOMAKE, $AUTOCONF, $ACLOCAL, $LIBTOOL, $LIBTOOLIZE"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=graphviz
TEST_TYPE=-f
FILE=graph/graph.h

DIE=0

($LIBTOOL --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have libtool installed to compile $PROJECT."
        echo "Get ftp://alpha.gnu.org/gnu/libtool-1.2d.tar.gz"
        echo "(or a newer version if it is available)"
        DIE=1
}

($AUTOMAKE --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.4.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

($AUTOCONF --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

rm -f missing depcomp mkinstalldirs install-sh ltmain.sh \
	config.sub config.guess ylwrap \
	config/config.guess config/config.sub config/ltmain.sh \
	config/install-sh config/mkinstalldirs config/missing \
	config/depcomp config/ylwrap

mkdir -p config

$LIBTOOLIZE --force --copy

$ACLOCAL -I m4 $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

# don't need default COPYING.  This is to suppress the automake message
touch COPYING

$AUTOMAKE --add-missing --copy $am_opt
$AUTOCONF
cd $ORIGDIR

# ensure depcomp exists even if still using automake-1.4
# otherwise "make dist" fails.
touch depcomp

# ensure COPYING is based on LICENSE.html
rm -f COPYING
lynx -dump LICENSE.html >COPYING

echo 
echo "Now type './configure' to configure $PROJECT."
