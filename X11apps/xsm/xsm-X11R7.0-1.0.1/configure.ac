
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
AC_INIT(xsm,[1.0.1], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xsm)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

if [[ -z $RSH ]] ; then
   AC_PATH_PROG(RSH,rsh)
fi
if [[ -z $RSH ]] ; then
   AC_PATH_PROG(RSH,rcmd)
fi
if [[ -z $RSH ]] ; then
   AC_PATH_PROG(RSH,remsh)
fi
if [[ -z $RSH ]] ; then
   AC_MSG_ERROR(No rsh like program found)
fi

#if !defined(RshCmd)
#if SystemV
#ifdef HPArchitecture
#define RshCmd remsh
#else
#define RshCmd rcmd
#endif
#else
#define RshCmd rsh
#endif
#endif

AC_CHECK_FUNC([mkstemp], 
   AC_DEFINE(HAS_MKSTEMP,1,[Define to 1 if you have the `mkstemp' function.]))
AC_CHECK_FUNC([putenv], [],
   AC_DEFINE(NOPUTENV,1,[Define to 1 if you DO NOT have the `putenv' function.]))

# Checks for pkg-config packages
PKG_CHECK_MODULES(XSM, x11 xt ice sm)
PKG_CHECK_MODULES(XSM_XAW, xaw7)
XSM_CFLAGS="$XSM_XAW_CFLAGS $XSM_CFLAGS"
XSM_LIBS="$XSM_XAW_LIBS $XSM_LIBS"

AC_SUBST(XSM_CFLAGS)
AC_SUBST(XSM_LIBS)

PKG_CHECK_MODULES(APPDEFS, xt)
appdefaultdir=$(pkg-config --variable=appdefaultdir xt)
AC_SUBST(appdefaultdir)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
