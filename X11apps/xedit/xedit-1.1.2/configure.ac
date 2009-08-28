
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
AC_INIT(xedit,[1.1.2], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xedit)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE(xprint,
	      AS_HELP_STRING([--enable-xprint],
			     [Compile with xprint support (default: disabled)]),
	      [enable_xprint=$enableval], [enable_xprint=no])
AM_CONDITIONAL(USE_XPRINT, test x$enable_xprint = xyes)
if test x$enable_xprint = xyes; then
   PKG_CHECK_MODULES(PKGDEPS, xprintutil xp xaw8)
else
   PKG_CHECK_MODULES(PKGDEPS, xaw7)
fi

AC_ARG_WITH(lispdir, AS_HELP_STRING([--with-lispdir=PATH],
	    [Directory to install lisp files (default: $libdir/X11/xedit/lisp)]),
	    [ lispdir="$withval"],
	    [ lispdir=${libdir}/X11/xedit/lisp ])
LISPDIR=$lispdir
AC_SUBST(LISPDIR)

AC_CHECK_FUNC(realpath, [], [have_realpath=yes])
AM_CONDITIONAL(NEED_REALPATH, test x$have_realpath = xyes)
AC_CHECK_FUNC(strcasecmp, [], [have_strcasecmp=yes])
AM_CONDITIONAL(NEED_STRCASECMP, test x$have_strcasecmp = xyes)
AC_CHECK_FUNC(unsetenv, [], [have_unsetenv=yes])
AM_CONDITIONAL(NEED_UNSETENV, test x$have_unsetenv = xyes)

AC_SUBST(PKGDEPS_CFLAGS)
AC_SUBST(PKGDEPS_LIBS)

PKG_CHECK_MODULES(APPDEFS, xt)
appdefaultdir=$(pkg-config --variable=appdefaultdir xt)
AC_SUBST(appdefaultdir)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
