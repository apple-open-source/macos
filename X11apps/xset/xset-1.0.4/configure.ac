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
AC_INIT(xset,[1.0.4], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xset)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

# Checks for system functions
AC_CHECK_FUNCS([usleep])

# Checks for pkg-config packages
PKG_CHECK_MODULES(XSET, xmuu)

# This is used below to allow <X11/Xlib.h> to be found
PKG_CHECK_MODULES(SET_X11, x11)

PKG_CHECK_MODULES(SET_XEXT, xext,
	[SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SET_XEXT_CFLAGS $SET_X11_CFLAGS"
	AC_CHECK_HEADERS([X11/extensions/dpms.h X11/extensions/MITMisc.h],,,[#include <X11/Xlib.h>])
	CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])

PKG_CHECK_MODULES(SET_XKB, x11, 
	[SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SET_XKB_CFLAGS $SET_X11_CFLAGS"
	AC_CHECK_HEADERS([X11/XKBlib.h],,,[#include <X11/Xlib.h>])
	CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])

PKG_CHECK_MODULES(SET_XF86MISC, xxf86misc, 
	[SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SET_XF86MISC_CFLAGS $SET_X11_CFLAGS"
	AC_CHECK_HEADERS([X11/extensions/xf86misc.h X11/extensions/xf86mscstr.h],,,
			  [#include <X11/Xlib.h>
#include <X11/Xproto.h>])
	CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])

PKG_CHECK_MODULES(SET_FONTCACHE, xfontcache,
	[SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SET_FONTCACHE_CFLAGS $SET_X11_CFLAGS"
	AC_CHECK_HEADERS([X11/extensions/fontcache.h X11/extensions/fontcacheP.h],,,[#include <X11/Xlib.h>])
	CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])

PKG_CHECK_MODULES(SET_XPRINT, xp, 
	[SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $SET_XPRINT_CFLAGS $SET_X11_CFLAGS"
	AC_CHECK_HEADERS([X11/extensions/Print.h],,,[#include <X11/Xlib.h>])
	CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])

XSET_CFLAGS="$XSET_CFLAGS $SET_XEXT_CFLAGS $SET_XKB_CFLAGS $SET_XF86MISC_CFLAGS $SET_FONTCACHE_CFLAGS $SET_XPRINT_CFLAGS"
XSET_LIBS="$XSET_LIBS $SET_XEXT_LIBS $SET_XKB_LIBS $SET_XF86MISC_LIBS $SET_FONTCACHE_LIBS $SET_XPRINT_LIBS"

AC_SUBST(XSET_CFLAGS)
AC_SUBST(XSET_LIBS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
