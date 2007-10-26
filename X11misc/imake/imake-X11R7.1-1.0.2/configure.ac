
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
AC_INIT(imake, [1.0.2], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

AC_PROG_CPP
AC_PATH_PROG(SED, sed)
XORG_PROG_RAWCPP
CPP_PROGRAM=${RAWCPP}
AC_SUBST(CPP_PROGRAM)

AC_CHECK_PROG(HAS_PERL, perl, yes)
AM_CONDITIONAL([HAS_PERL], [test "x$HAS_PERL" = xyes])

m4_ifdef([AS_HELP_STRING], , [m4_define([AS_HELP_STRING], m4_defn([AC_HELP_STRING]))])

DEFAULT_XCONFDIR="${libdir}/X11/config"
AC_ARG_WITH(config-dir,
	AS_HELP_STRING([--with-config-dir=<path>], [Path to config dir (default: ${libdir}/X11/config)]),
	[XCONFDIR="$withval"],
	[XCONFDIR="$DEFAULT_XCONFDIR"])
AC_SUBST(XCONFDIR)

DEFAULT_PREPROCESSCMD_MKDEPEND="gcc -E"
AC_ARG_WITH(script-preproc-cmd,
	AS_HELP_STRING([--with-script-preproc-cmd=CMD], [Preprocessor command to run on scripts (default: "gcc -E")]),
	[PREPROCESSCMD_MKDEPEND="$withval"],
	[PREPROCESSCMD_MKDEPEND="$DEFAULT_PREPROCESSCMD_MKDEPEND"])
AC_SUBST(PREPROCESSCMD_MKDEPEND)

DEFAULT_ARCMD="ar clq"
AC_ARG_WITH(create-lib-cmd,
	AS_HELP_STRING([--with-create-lib-cmd=CMD], [Command to create libraries (default: "ar clq")]),
	[ARCMD="$withval"],
	[ARCMD="$DEFAULT_ARCMD"])
AC_SUBST(ARCMD)

DEFAULT_RANLIB="ranlib"
AC_ARG_WITH(clean-lib-cmd,
	AS_HELP_STRING([--with-clean-lib-cmd=CMD], [Command to clean up libraries (default: "ranlib")]),
	[RANLIB="$withval"],
	[RANLIB="$DEFAULT_RANLIB"])
AC_SUBST(RANLIB)

AC_ARG_ENABLE(revpath,
	AS_HELP_STRING([--disable-revpath], [Build revpath (default: enabled)]),
	[BUILD_REVPATH="$enableval"],
	[BUILD_REVPATH=yes])
AM_CONDITIONAL([BUILD_REVPATH], [test "x$BUILD_REVPATH" = xyes])

AC_ARG_ENABLE(makeg,
	AS_HELP_STRING([--disable-makeg], [Build makeg (default: enabled)]),
	[BUILD_MAKEG="$enableval"],
	[BUILD_MAKEG=yes])
AM_CONDITIONAL([BUILD_MAKEG], [test "x$BUILD_MAKEG" = xyes])

AC_ARG_ENABLE(xmkmf,
	AS_HELP_STRING([--disable-xmkmf], [Build xmkmf (default: enabled)]),
	[BUILD_XMKMF="$enableval"],
	[BUILD_XMKMF=yes])
AM_CONDITIONAL([BUILD_XMKMF], [test "x$BUILD_XMKMF" = xyes])

AC_ARG_ENABLE(ccmakedep,
	AS_HELP_STRING([--disable-ccmakedep], [Build ccmakedep (default: enabled)]),
	[BUILD_CCMAKEDEP="$enableval"],
	[BUILD_CCMAKEDEP=yes])
AM_CONDITIONAL([BUILD_CCMAKEDEP], [test "x$BUILD_CCMAKEDEP" = xyes])

AC_ARG_ENABLE(mergelib,
	AS_HELP_STRING([--disable-mergelib], [Build mergelib (default: enabled)]),
	[BUILD_MERGELIB="$enableval"],
	[BUILD_MERGELIB=yes])
AM_CONDITIONAL([BUILD_MERGELIB], [test "x$BUILD_MERGELIB" = xyes])

AC_ARG_ENABLE(mkdirhier,
	AS_HELP_STRING([--disable-mkdirhier], [Build mkdirhier (default: enabled)]),
	[BUILD_MKDIRHIER="$enableval"],
	[BUILD_MKDIRHIER=yes])
AM_CONDITIONAL([BUILD_MKDIRHIER], [test "x$BUILD_MKDIRHIER" = xyes])

AC_ARG_ENABLE(cleanlinks,
	AS_HELP_STRING([--disable-cleanlinks], [Build cleanlinks (default: enabled)]),
	[BUILD_CLEANLINKS="$enableval"],
	[BUILD_CLEANLINKS=yes])
AM_CONDITIONAL([BUILD_CLEANLINKS], [test "x$BUILD_CLEANLINKS" = xyes])

AC_ARG_ENABLE(mkhtmlindex,
	AS_HELP_STRING([--disable-mkhtmlindex], [Build mkhtmlindex (default: enabled)]),
	[BUILD_MKHTMLINDEX="$enableval"],
	[BUILD_MKHTMLINDEX=yes])
AM_CONDITIONAL([BUILD_MKHTMLINDEX], [test "x$BUILD_MKHTMLINDEX" = xyes])

# Checks for pkg-config packages
PKG_CHECK_MODULES(XPROTO, xproto)
AC_SUBST(XPROTO_CFLAGS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
