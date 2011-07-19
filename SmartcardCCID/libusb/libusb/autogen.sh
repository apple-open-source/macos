#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=libusb
TEST_TYPE=-f
FILE=usb.h.in

DIE=0

(autoconf${AUTOCONF_SUFFIX} --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(automake${AUTOMAKE_SUFFIX} --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://sourceware.cygnus.com/pub/automake/automake-1.4.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

# libtoolize on Darwin systems is glibtoolize
(glibtoolize --version) < /dev/null > /dev/null 2>&1 && LIBTOOLIZE=glibtoolize || LIBTOOLIZE=libtoolize

$LIBTOOLIZE --force
aclocal${AUTOMAKE_SUFFIX} $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader${AUTOCONF_SUFFIX} --version)  < /dev/null > /dev/null 2>&1 && autoheader${AUTOCONF_SUFFIX}

automake${AUTOMAKE_SUFFIX} -a $am_opt
autoconf${AUTOCONF_SUFFIX}
cd $ORIGDIR

## Fix the makefile
#sed -e 's/^\(all:.*\)/\1\
#	echo "s|\\(ECHO=\\(.*\\)\\)|echo=\\2\\\\" > echoFix.sed \
#	echo " \\1|" >> echoFix.sed \
#	sed -f echoFix.sed -i .bak libtool/' -i .bak Makefile.in | exit 1

if [ "$1" == "--skip-configure" ]; then
	exit
fi

$srcdir/configure --enable-maintainer-mode "$@" || exit

echo 
echo "Now type 'make' to compile $PROJECT."
