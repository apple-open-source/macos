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
AC_INIT([xfs],
        [1.1.1],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [xfs])
# Set common system defines for POSIX extensions, such as _GNU_SOURCE
# Must be called before any macros that run the compiler (like AC_PROG_LIBTOOL)
AC_USE_SYSTEM_EXTENSIONS
# xfs already have a header called config.h
AC_CONFIG_HEADERS([xfs-config.h])
AC_CONFIG_MACRO_DIR([m4])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require xorg-macros minimum of 1.10 for HAVE_STYLESHEETS in XORG_CHECK_SGML_DOCTOOLS
m4_ifndef([XORG_MACROS_VERSION],
	  [m4_fatal([must install xorg-macros 1.10 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.10)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_DEVEL_DOCS
XORG_WITH_XMLTO(0.0.20)
XORG_WITH_FOP
XORG_CHECK_SGML_DOCTOOLS(1.5)
XORG_WITH_LINT

AC_CHECK_HEADERS([stdint.h])

AC_ARG_WITH(default-config-file,
	AS_HELP_STRING([--with-default-config-file=PATH],
   [comma-separated list of paths to look for config file when not specified (default: ${sysconfdir}/X11/fs/config)]),
	[CONFIG_FILE="$withval"], [CONFIG_FILE="${sysconfdir}/X11/fs/config"])

DEFAULT_CONFIG_FILE=`echo ${CONFIG_FILE} | sed 's/,/\\\\\\\\\\\",\\\\\\\\\\\"/'g`
AX_DEFINE_DIR([DEFAULT_CONFIG_FILE], DEFAULT_CONFIG_FILE,
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

# Require X.Org's font util macros 1.1 or later for XORG_FONTROOTDIR
m4_ifndef([XORG_FONT_MACROS_VERSION],
	  [m4_fatal([must install X.Org font-util 1.1 or later before running autoconf/autogen])])
XORG_FONT_MACROS_VERSION(1.1)

XORG_FONTROOTDIR

# Determine font path to put in config file
# Uses --default-font-path if set, otherwise checks for /etc/X11/fontpath.d,
# otherwise uses default path copied from xserver configure.ac
AC_CHECK_FILE([${sysconfdir}/X11/fontpath.d],
	[DEFAULT_FONT_PATH='catalogue:$(sysconfdir)/X11/fontpath.d'],
	[
	DEFAULT_FONT_PATH='$(FONTROOTDIR)/misc/,$(FONTROOTDIR)/TTF/,$(FONTROOTDIR)/OTF/,$(FONTROOTDIR)/Type1/,$(FONTROOTDIR)/100dpi/,$(FONTROOTDIR)/75dpi/'
	case $host_os in
        	darwin*)    DEFAULT_FONT_PATH="${DEFAULT_FONT_PATH},/Library/Fonts,/System/Library/Fonts" ;;
	esac
])

AC_ARG_WITH(default-font-path, AS_HELP_STRING([--with-default-font-path=PATH], [Comma separated list of font path entries]),
                                [ FONTPATH="$withval" ],
                                [ FONTPATH="${DEFAULT_FONT_PATH}" ])
AC_SUBST([FONTPATH])

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

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

AC_CONFIG_FILES([Makefile
                 config/Makefile
                 doc/Makefile
                 man/Makefile])
AC_OUTPUT
