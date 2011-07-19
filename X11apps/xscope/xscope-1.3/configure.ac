dnl
dnl Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.
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
AC_INIT([xscope], [1.3],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [xscope])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)

AC_USE_SYSTEM_EXTENSIONS
XORG_DEFAULT_OPTIONS

AC_ARG_ENABLE(xtrans,    AS_HELP_STRING([--enable-xtrans],
                                        [Use xtrans for socket connections (default: auto)]),
              [XTRANS=$enableval], [XTRANS=auto])

if test "x$XTRANS" = xauto ; then
	PKG_CHECK_EXISTS(xtrans, [XTRANS=yes], [XTRANS=no])
fi

if test "x$XTRANS" = xyes ; then
	dnl Issue an error if xtrans.m4 was not found and XTRANS_CONNECTION_FLAGS macro
	dnl was not expanded, since xscope with no transport types is rather useless.
	dnl
	dnl If you're seeing an error here, be sure you installed the lib/xtrans module
	dnl first and if it's not in the default location, that you set the ACLOCAL
	dnl environment variable to find it, such as:
	dnl     ACLOCAL="aclocal -I ${PREFIX}/share/aclocal"
	m4_pattern_forbid([XTRANS_CONNECTION_FLAGS])

	# Transport selection macro from xtrans.m4
	XTRANS_CONNECTION_FLAGS

	# Checks for pkg-config packages
	PKG_CHECK_MODULES(XSCOPE, xtrans)
	AC_DEFINE([USE_XTRANS], [1], [Use xtrans for socket connection code])
fi

AC_CHECK_FUNCS([getdtablesize])
AC_CHECK_HEADERS([sys/filio.h])

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT

AC_OUTPUT([Makefile])
