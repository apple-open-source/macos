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
AC_INIT(xlogo,[1.0.1], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xlogo)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

# Checks for pkg-config packages
XAW_CHECK_XPRINT_SUPPORT(XLOGO)

if test "x$xaw_use_xprint" = "xyes" ; then
   PKG_CHECK_MODULES(XPRINT_UTIL, xprintutil xp)

   XLOGO_CFLAGS="$XLOGO_CFLAGS $XPRINT_UTIL_CFLAGS"
   XLOGO_LIBS="$XLOGO_LIBS $XPRINT_UTIL_LIBS"
fi

# Use render & xft2 to draw logo?

AC_ARG_WITH(render, 
	AC_HELP_STRING([--with-xrender], [Use Xrender & Xft2 to draw logo]), 
	USE_RENDER="$withval", USE_RENDER="try")

if test "x$USE_RENDER" = "xtry" ; then
	PKG_CHECK_EXISTS([xrender xft], USE_RENDER="yes", USE_RENDER="no")
fi

if test "x$USE_RENDER" = "xyes" ; then
	PKG_CHECK_MODULES(XRENDER, xrender)
	PKG_CHECK_MODULES(XFT, xft)

	AC_DEFINE(XRENDER, 1, [Define to 1 to use Xrender & Xft2 to draw logo])

	XLOGO_CFLAGS="$XLOGO_CFLAGS $XRENDER_CFLAGS $XFT_CFLAGS"
	XLOGO_LIBS="$XLOGO_LIBS $XRENDER_LIBS $XFT_LIBS"
fi

PKG_CHECK_MODULES(XEXT, xext)
XLOGO_CFLAGS="$XLOGO_CFLAGS $XEXT_CFLAGS"
XLOGO_LIBS="$XLOGO_LIBS $XEXT_LIBS"

AC_SUBST(XLOGO_CFLAGS)
AC_SUBST(XLOGO_LIBS)

PKG_CHECK_MODULES(APPDEFS, xt)
appdefaultdir=$(pkg-config --variable=appdefaultdir xt)
AC_SUBST(appdefaultdir)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
