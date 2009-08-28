dnl  Copyright 2005 Red Hat, Inc.
dnl 
dnl  Permission to use, copy, modify, distribute, and sell this software and its
dnl  documentation for any purpose is hereby granted without fee, provided that
dnl  the above copyright notice appear in all copies and that both that
dnl  copyright notice and this permission notice appear in supporting
dnl  documentation, and that the name of Red Hat not be used in
dnl  advertising or publicity pertaining to distribution of the software without
dnl  specific, written prior permission.  Red Hat makes no
dnl  representations about the suitability of this software for any purpose.  It
dnl  is provided "as is" without express or implied warranty.
dnl 
dnl  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl  PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
AC_INIT(xrdb,[1.0.5], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xrdb)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_FUNC([mkstemp], 
   AC_DEFINE(HAS_MKSTEMP,1,[Define to 1 if you have the `mkstemp' function.]))

# Find MAXHOSTNAMELEN definition
# Common hidey holes:
#  BSD & Linux - <sys/param.h>
#  Solaris - <netdb.h>

AC_CHECK_DECL([MAXHOSTNAMELEN],[FOUND_MAXHOSTNAMELEN=yes])

if test x$FOUND_MAXHOSTNAMELEN != xyes ; then
	AC_MSG_CHECKING([for header that defines MAXHOSTNAMELEN])

	FOUND_MAXHOSTNAMELEN='not found'

	AC_COMPILE_IFELSE(
	    AC_LANG_PROGRAM([#include <sys/param.h>],
			    [int h = MAXHOSTNAMELEN;]),
	    [FOUND_MAXHOSTNAMELEN='sys/param.h'
	     AC_DEFINE(NEED_SYS_PARAM_H,1,
	       [Define to 1 if you need <sys/param.h> to define MAXHOSTNAMELEN])])

	AC_COMPILE_IFELSE(
	    AC_LANG_PROGRAM([#include <netdb.h>],
			    [int h = MAXHOSTNAMELEN;]),
	    [FOUND_MAXHOSTNAMELEN='netdb.h'
	     AC_DEFINE(NEED_NETDB_H,1,
	       [Define to 1 if you need <netdb.h> to define MAXHOSTNAMELEN])])

	AC_MSG_RESULT([$FOUND_MAXHOSTNAMELEN])
fi

# xrdb needs to know where to find cpp at runtime - previously set as CppCmd
# in Imake config files for each OS
AC_ARG_WITH([cpp], 
	AC_HELP_STRING([--with-cpp=path],
		       [path to cpp command for xrdb to use at runtime]), 
	[AC_MSG_CHECKING(for cpp)
	CPP_PATH=$withval
	AC_MSG_RESULT($CPP_PATH)], 
	[AC_PATH_PROG([CPP_PATH],[cpp], [cpp], 
 [$PATH:/bin:/usr/bin:/usr/lib:/usr/libexec:/usr/ccs/lib:/usr/ccs/lbin:/lib])])
AC_DEFINE_UNQUOTED([CPP], "$CPP_PATH", [Path to CPP program])

# Checks for pkg-config packages
PKG_CHECK_MODULES(XRDB, xmuu x11)
AC_SUBST(XRDB_CFLAGS)
AC_SUBST(XRDB_LIBS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
