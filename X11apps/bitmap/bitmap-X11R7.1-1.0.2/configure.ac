
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
AC_INIT(bitmap,[1.0.2], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],bitmap)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_FUNC([mkstemp], 
   AC_DEFINE(HAS_MKSTEMP,1,[Define to 1 if you have the `mkstemp' function.]))

# bmtoa checks for packages
PKG_CHECK_MODULES(BMTOA, x11 xmu)
AC_SUBST(BMTOA_CFLAGS)
AC_SUBST(BMTOA_LIBS)

# atobm checks for packages
PKG_CHECK_MODULES(ATOBM, x11)
AC_SUBST(ATOBM_CFLAGS)
AC_SUBST(ATOBM_LIBS)

# bitmap checks for packages
XAW_CHECK_XPRINT_SUPPORT(BITMAP)
PKG_CHECK_MODULES(XBITMAPS, xbitmaps)

BITMAP_CFLAGS="$BITMAP_CFLAGS $XBITMAPS_CFLAGS"
BITMAP_LIBS="$BITMAP_LIBS $XBITMAPS_LIBS"

AC_SUBST(BITMAP_CFLAGS)
AC_SUBST(BITMAP_LIBS)



PKG_CHECK_MODULES(APPDEFS, xt)
appdefaultdir=$(pkg-config --variable=appdefaultdir xt)
AC_SUBST(appdefaultdir)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
