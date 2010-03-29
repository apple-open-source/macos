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
AC_INIT(xfs,[1.1.0], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xfs)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

# xfs already have a header called config.h
AM_CONFIG_HEADER(xfs-config.h)

# Require xorg-macros version >= 1.2.0 for XORG_CWARNFLAGS & XORG_CHANGELOG
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.2 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.2)

# Set common system defines for POSIX extensions, such as _GNU_SOURCE
# Must be called before any macros that run the compiler (like AC_PROG_LIBTOOL)
# to avoid autoconf errors.
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
XORG_PROG_RAWCPP
XORG_CWARNFLAGS
AC_PROG_INSTALL
AC_CANONICAL_HOST

# xmlto is used to convert doc/design.xml from DocBook to PDF/HTML
AC_ARG_VAR([XMLTO], [Path to xmlto command])
AC_PATH_PROG([XMLTO], [xmlto])
AM_CONDITIONAL([HAVE_XMLTO], [test "x$XMLTO" != "x"])

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

# Determine font path to put in config file
# Uses --default-font-path if set, otherwise checks for /etc/X11/fontpath.d,
# otherwise uses default path copied from xserver configure.ac
AC_CHECK_FILE([${sysconfdir}/X11/fontpath.d],
	[DEFAULT_FONT_PATH='catalogue:$(sysconfdir)/X11/fontpath.d'],
	[
	DEFAULT_FONT_PATH='$(FONTDIR)/misc/,$(FONTDIR)/TTF/,$(FONTDIR)/OTF/,$(FONTDIR)/Type1/,$(FONTDIR)/100dpi/,$(FONTDIR)/75dpi/'
	case $host_os in
        	darwin*)    DEFAULT_FONT_PATH="${DEFAULT_FONT_PATH},/Library/Fonts,/System/Library/Fonts" ;;
	esac
])

AC_ARG_WITH(default-font-path, AS_HELP_STRING([--with-default-font-path=PATH], [Comma separated list of font path entries]),
                                [ FONTPATH="$withval" ],
                                [ FONTPATH="${DEFAULT_FONT_PATH}" ])
AC_SUBST([FONTPATH])

# Option to enable support for building developer internals docs
AC_ARG_ENABLE(devel-docs,
	AS_HELP_STRING([--enable-devel-docs],
	    [Build internals documentation for developers (default: enabled)]),
	    [DEVEL_DOCS=$enableval], [DEVEL_DOCS=yes])
AM_CONDITIONAL([MAKE_DEVEL_DOCS], [test "x$DEVEL_DOCS" = "xyes"])

# Option to enable support for starting from inetd
AC_ARG_ENABLE(inetd,
	AS_HELP_STRING([--enable-inetd],
	    [Support -inetd flag for starting from inetd (default: enabled)]),
	    [INETD=$enableval], [INETD=yes])
if test "x$INETD" = xyes; then
        AC_DEFINE(XFS_INETD, 1, [Build support for starting from inetd])
fi

# Option to enable support for logging to syslog
AC_ARG_ENABLE(syslog,
	AS_HELP_STRING([--enable-syslog],
	    [Support option for logging via syslog (default: enabled)]),
	    [SYSLOG=$enableval], [SYSLOG=yes])
if test "x$SYSLOG" = xyes; then
        AC_DEFINE(USE_SYSLOG, 1, [Build support for logging via syslog])
fi


# Checks for system functions / libraries
AC_CHECK_FUNCS([daemon])

# Checks for pkg-config packages
PKG_CHECK_MODULES(XFS, libfs xfont xtrans)
AC_SUBST(XFS_CFLAGS)
AC_SUBST(XFS_LIBS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION
XORG_CHANGELOG

AC_OUTPUT([Makefile])
