dnl 
dnl  $Id$
dnl 
dnl  Copyright © 2003 Keith Packard
dnl 
dnl  Permission to use, copy, modify, distribute, and sell this software and its
dnl  documentation for any purpose is hereby granted without fee, provided that
dnl  the above copyright notice appear in all copies and that both that
dnl  copyright notice and this permission notice appear in supporting
dnl  documentation, and that the name of Keith Packard not be used in
dnl  advertising or publicity pertaining to distribution of the software without
dnl  specific, written prior permission.  Keith Packard makes no
dnl  representations about the suitability of this software for any purpose.  It
dnl  is provided "as is" without express or implied warranty.
dnl 
dnl  KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl  EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl  PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
dnl
dnl This is the package version number, not the shared library
dnl version.  This same version number must appear in Xcursor.h
dnl Yes, it is a pain to synchronize version numbers.  Unfortunately, it's
dnl not possible to extract the version number here from Xcursor.h
dnl
AC_INIT([libXcursor],1.1.9,[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],[libXcursor])
AM_INIT_AUTOMAKE([dist-bzip2])
AC_CONFIG_SRCDIR([Makefile.am])
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

# Require xorg-macros version 1.1.0 or newer for XORG_WITH_LINT macro
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.1 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.1)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

AC_ARG_WITH(icondir,
        AC_HELP_STRING([--with-icondir=<path>],
                       [Set default icon directory (default: ${datadir}/icons)]),
        [ICONDIR="$withval"],
        [ICONDIR=${datadir}/icons])
AC_SUBST([ICONDIR])

DEF_CURSORPATH="~/.icons:${datadir}/icons:${datadir}/pixmaps"
if test "x${ICONDIR}" != "x${datadir}/icons"; then
	DEF_CURSORPATH="${DEF_CURSORPATH}:${ICONDIR}"
fi
AC_ARG_WITH(cursorpath,
        AC_HELP_STRING([--with-cursorpath=<paths>],
                       [Set default search path for cursors]),
        [XCURSORPATH="$withval"],
        [XCURSORPATH=$DEF_CURSORPATH])
AC_SUBST([XCURSORPATH])

# Reformat cursor path for man page
XCURSORPATH_LIST=`echo $XCURSORPATH | sed 's/:/, /g'`
AC_SUBST([XCURSORPATH_LIST])

# Check for X
PKG_CHECK_MODULES(XCURSOR, xrender >= 0.8.2 xfixes x11 fixesproto)
AC_DEFINE(HAVE_XFIXES, 1, [Define to 1 if you have Xfixes])

AC_SUBST(XCURSOR_CFLAGS)
AC_SUBST(XCURSOR_LIBS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT

AC_OUTPUT([Makefile
	   src/Makefile
	   man/Makefile
           xcursor.pc])
