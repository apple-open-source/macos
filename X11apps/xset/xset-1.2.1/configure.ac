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

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([xset], [1.2.1],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [xset])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require xorg-macros: XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

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

AC_ARG_WITH(xf86misc, AC_HELP_STRING([--without-xf86misc],[Disable xf86misc support.]),
                [USE_XF86MISC="$withval"], [USE_XF86MISC="yes"])
if test "x$USE_XF86MISC" != "xno" ; then
	PKG_CHECK_MODULES(SET_XF86MISC, xxf86misc,
		[SAVE_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $SET_XF86MISC_CFLAGS $SET_X11_CFLAGS"
		AC_CHECK_HEADERS([X11/extensions/xf86misc.h X11/extensions/xf86mscstr.h],,,
				  [#include <X11/Xlib.h>
#include <X11/Xproto.h>])
		CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])
else
        echo "without xf86misc"
fi

AC_ARG_WITH(fontcache, AC_HELP_STRING([--without-fontcache],[Disable fontcache support.]),
                [USE_XFONTCACHE="$withval"], [USE_XFONTCACHE="yes"])
if test "x$USE_XFONTCACHE" != "xno" ; then
	PKG_CHECK_MODULES(SET_FONTCACHE, xfontcache,
		[SAVE_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $SET_FONTCACHE_CFLAGS $SET_X11_CFLAGS"
		AC_CHECK_HEADERS([X11/extensions/fontcache.h X11/extensions/fontcacheP.h],,,[#include <X11/Xlib.h>])
		CPPFLAGS="$SAVE_CPPFLAGS"],[echo "not found"])
else
	echo "without fontcache"
fi

XSET_CFLAGS="$XSET_CFLAGS $SET_XEXT_CFLAGS $SET_XKB_CFLAGS $SET_XF86MISC_CFLAGS $SET_FONTCACHE_CFLAGS"
XSET_LIBS="$XSET_LIBS $SET_XEXT_LIBS $SET_XKB_LIBS $SET_XF86MISC_LIBS $SET_FONTCACHE_LIBS"

AC_SUBST(XSET_CFLAGS)
AC_SUBST(XSET_LIBS)

AC_OUTPUT([Makefile])
