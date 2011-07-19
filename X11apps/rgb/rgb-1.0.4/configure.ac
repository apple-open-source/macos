dnl Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, including without limitation
dnl the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl and/or sell copies of the Software, and to permit persons to whom the
dnl Software is furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice (including the next
dnl paragraph) shall be included in all copies or substantial portions of the
dnl Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
dnl THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl DEALINGS IN THE SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.60])
AC_INIT([rgb], [1.0.4],
	[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [rgb])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
	  [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

PKG_CHECK_MODULES(RGB, xproto)

AC_MSG_CHECKING([for rgb database location])
AC_ARG_WITH([rgb-db-dir],
	AC_HELP_STRING([--with-rgb-db-dir=<path>],
		[rgb database location (default is ${datadir}/X11/rgb)]),
	[db_path=$withval], [db_path=${datadir}/X11/rgb])
AC_MSG_RESULT([$db_path])
dnl XXX not working - AC_DEFINE([RGB_DB], $db_path, [set to location of rgb database (without any file type suffix)])
db_file=`basename $db_path`
db_dir=`dirname $db_path`
AC_SUBST([db_file])
AC_SUBST([db_dir])

AC_MSG_CHECKING([for rgb database format])
AC_ARG_WITH([rgb-db-type],
	AC_HELP_STRING([--with-rgb-db-type=(text|dbm|ndbm)],
		[rgb database type (default is text)]),
	[db_type=$withval], [db_type="text"])
AC_MSG_RESULT([$db_type])

RGB_DB_TYPE=$db_type

case $db_type in
	text)
		RGB_DB_FILES=""
		AC_DEFINE([USE_RGB_TXT], [1],
			[Define to 1 to use plain text files for rgb database])
		;;
	dbm)
		AC_SEARCH_LIBS([dbminit], [db dbm nsl], [],
				AC_MSG_ERROR([dbm requested but dbminit() not found in any libraries]))
		AC_CHECK_HEADER([dbm.h], [DBM_HEADER='<dbm.h>'],
		  [AC_CHECK_HEADER([rpcsvc/dbm.h], [DBM_HEADER='<rpcsvc/dbm.h>'],
		    [AC_MSG_ERROR([dbm requested but dbm.h not found])])])
		PKG_CHECK_MODULES(XORG, [xorg-server])
		RGB_CFLAGS="$RGB_CFLAGS $XORG_CFLAGS"
		RGB_DB_FILES='$(db_file).dir $(db_file).pag'
		;;
	ndbm)
		# Find a dbm or ndbm implementation
		AC_SEARCH_LIBS([dbm_open], [db ndbm dbm],
				AC_DEFINE([NDBM], [1],
				 [Define to 1 if you have ndbm.h interfaces]),
				AC_MSG_ERROR([ndbm requested but dbm_open() not found in any libraries]))
		DBM_HEADER='<ndbm.h>'
		PKG_CHECK_MODULES(XORG, [xorg-server])
		RGB_CFLAGS="$RGB_CFLAGS $XORG_CFLAGS"
		RGB_DB_FILES='$(db_file).dir $(db_file).pag'
		;;
esac
if test x$DBM_HEADER != x ; then
  AC_DEFINE_UNQUOTED([DBM_HEADER], [$DBM_HEADER],
		     [Header file to include for dbm functions])
fi
AC_SUBST([RGB_DB_TYPE])
AC_SUBST([RGB_DB_FILES])
AM_CONDITIONAL(RGB_DB, [test x$db_type != xtext])
AC_SUBST(RGB_CFLAGS)

AC_CONFIG_FILES([Makefile
                 man/Makefile])

AC_OUTPUT
