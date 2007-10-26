dnl Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the
dnl "Software"), to deal in the Software without restriction, including
dnl without limitation the rights to use, copy, modify, merge, publish,
dnl distribute, and/or sell copies of the Software, and to permit persons
dnl to whom the Software is furnished to do so, provided that the above
dnl copyright notice(s) and this permission notice appear in all copies of
dnl the Software and that both the above copyright notice(s) and this
dnl permission notice appear in supporting documentation.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
dnl OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
dnl HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
dnl INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
dnl FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
dnl NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
dnl WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Except as contained in this notice, the name of a copyright holder
dnl shall not be used in advertising or otherwise to promote the sale, use
dnl or other dealings in this Software without prior written authorization
dnl of the copyright holder.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
AC_INIT(rgb,[1.0.1],[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],rgb)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

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

case $db_type in
	text)
		RGB_DB_FILES=""
		AC_DEFINE([USE_RGB_TXT], [1],
			[Define to 1 to use plain text files for rgb database])
		;;
	dbm)
		AC_SEARCH_LIBS([dbminit], [db dbm],  [],
				AC_MSG_ERROR([dbm requested but dbminit() not found in any libraries]))
		RGB_DB_FILES="$(db_file).dir $(db_file).pag"
		;;
	ndbm)
		# Find a dbm or ndbm implementation
		AC_SEARCH_LIBS([dbm_open], [db ndbm dbm],  
				AC_DEFINE([NDBM], [1], 
				 [Define to 1 if you have ndbm.h interfaces]),
				AC_MSG_ERROR([ndbm requested but dbm_open() not found in any libraries]))
		RGB_DB_FILES="$(db_file).dir $(db_file).pag"
		;;
esac
AC_SUBST([RGB_DB_FILES])
AM_CONDITIONAL(RGB_TXT, [test x$db_type = xtext])

PKG_CHECK_MODULES(RGB, xproto)
AC_SUBST(RGB_CFLAGS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
