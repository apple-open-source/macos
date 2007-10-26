
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
AC_INIT(xrx,[1.0.1], [xorg@freedesktop.org],xrx)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

dnl Issue an error if xtrans.m4 was not found and XTRANS_CONNECTION_FLAGS macro
dnl was not expanded, since libX11 with no transport types is rather useless.
dnl
dnl If you're seeing an error here, be sure you installed the lib/xtrans module
dnl first and if it's not in the default location, that you set the ACLOCAL
dnl environment variable to find it, such as:
dnl     ACLOCAL="aclocal -I ${PREFIX}/share/aclocal"
m4_pattern_forbid(XTRANS_CONNECTION_FLAGS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

# Checks for pkg-config packages
PKG_CHECK_MODULES(XRX, x11 xt xext xtrans xproxymngproto xau)
AC_SUBST(XRX_CFLAGS)
AC_SUBST(XRX_LIBS)

XAW_CHECK_XPRINT_SUPPORT(XRX_PLUGIN)
AC_SUBST(XRX_PLUGIN_CFLAGS)
AC_SUBST(XRX_PLUGIN_LIBS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([
	Makefile
	helper/Makefile
	plugin/Makefile
	libxplugin/Makefile
	htdocs/Makefile
	cgi-bin/Makefile
	xnest-plugin/Makefile
	testplugin/Makefile
	rx/Makefile
	plugin/common/Makefile
	plugin/include/Makefile
])
