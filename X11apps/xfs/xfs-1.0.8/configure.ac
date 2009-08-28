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
AC_INIT(xfs,[1.0.8], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xfs)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(xfs-config.h)

# Require xorg-macros version 1.1.0 or newer for XORG_WITH_LINT macro
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.1 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.1)

AC_PROG_CC
AC_PROG_INSTALL
AC_CANONICAL_HOST

case $host_os in
    linux*|gnu*|*-gnu)
        OS_CFLAGS="-D_XOPEN_SOURCE"
        ;;
    *)
        OS_CFLAGS=
        ;;
esac

AC_CHECK_HEADERS([stdint.h])

AC_ARG_WITH(default-config-file,
	AS_HELP_STRING([--with-default-config-file=PATH],
   [comma-separated list of paths to look for config file when not specified (default: ${sysconfdir}/X11/fs/config)]),
	[CONFIG_FILE="$withval"], [CONFIG_FILE="${sysconfdir}/X11/fs/config"])

DEFAULT_CONFIG_FILE=`echo ${CONFIG_FILE} | sed 's/,/\\\\\\\\\\\",\\\\\\\\\\\"/'g`
AC_DEFINE_DIR([DEFAULT_CONFIG_FILE], DEFAULT_CONFIG_FILE,
    [comma-separated list of strings for config file paths when not specified])

# generate text for the man page
case $CONFIG_FILE in
    *,*)
	CONFIG_FILE_DESC="the first file found from the list:" 
	CONFIG_FILE_PATH=`echo ${CONFIG_FILE} | sed 's/,/\\\\\\\\fR, \\\\\\\\fI/g'`
	;;
    *)
	CONFIG_FILE_DESC="the default file,"
	CONFIG_FILE_PATH="${CONFIG_FILE}"
	;;
esac
AC_SUBST([CONFIG_FILE_DESC])
AC_SUBST([CONFIG_FILE_PATH])

# Checks for pkg-config packages
PKG_CHECK_MODULES(XFS, libfs xfont xtrans)
XFS_CFLAGS="$XFS_CFLAGS $OS_CFLAGS"
AC_SUBST(XFS_CFLAGS)
AC_SUBST(XFS_LIBS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
