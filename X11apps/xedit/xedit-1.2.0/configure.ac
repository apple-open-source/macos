
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

AC_PREREQ([2.60])
AC_INIT([xedit], [1.2.0],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [xedit])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

AM_CONFIG_HEADER(config.h)

AM_PROG_CC_C_O
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(PKGDEPS, xaw7 xmu xt x11)

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

PKG_CHECK_MODULES(APPDEFS, xt)
xt_appdefaultdir=`$PKG_CONFIG --variable=appdefaultdir xt`
AC_ARG_WITH(appdefaultdir,
	AC_HELP_STRING([--with-appdefaultdir=<pathname>],
	  [specify directory for app-defaults files (default is autodetected)]),
	[appdefaultdir="$withval"], [appdefaultdir="${xt_appdefaultdir}"])
AC_SUBST(appdefaultdir)


AC_OUTPUT([Makefile])
