
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
AC_INIT(xman,[1.0.2], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xman)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_CANONICAL_HOST

AC_CHECK_FUNC([mkstemp], 
   AC_DEFINE(HAS_MKSTEMP,1,[Define to 1 if you have the `mkstemp' function.]))

AC_ARG_WITH(helpdir,
        AC_HELP_STRING([--with-helpdir=<path>],
                       [Set default directory for xman.help (default: ${datadir}/X11)]),
        [HELPDIR="$withval"],
        [HELPDIR=${datadir}/X11])
AC_SUBST([HELPDIR])

AC_ARG_WITH(sysmanpath,
        AC_HELP_STRING([--with-sysmanpath=<path>],
                       [Set default system man page search path]),
        [SYSMANPATH="$withval"], [])
if test x$SYSMANPATH != x; then
    AC_DEFINE_UNQUOTED(SYSMANPATH, "$SYSMANPATH", 
	[Default system man page search path (default: none)])
fi

AC_ARG_WITH(localmanpath,
        AC_HELP_STRING([--with-localmanpath=<path>],
                       [Set default local man page search path]),
        [LOCALMANPATH="$withval"], [])
if test x$LOCALMANPATH != x; then
    AC_DEFINE_UNQUOTED(LOCALMANPATH, "$LOCALMANPATH", 
	[Default local man page search path (default: none)])
fi

# Checks for pkg-config packages
XAW_CHECK_XPRINT_SUPPORT(XMAN)

# Check for man page config files
AC_CHECK_FILE([/etc/man.conf], [MANCONF="/etc/man.conf"], 
    AC_CHECK_FILE([/etc/man.config], [MANCONF="/etc/man.config"], 
	AC_CHECK_FILE([/etc/manpath.config], [MANCONF="/etc/manpath.config"])))
	
if test x$MANCONF != x ; then
    AC_DEFINE_UNQUOTED(MANCONF, "$MANCONF", 
	[Define to path to man config file if you have one])

    # Try to determine format of config file
    # would be better to somehow determine from the files themselves, but
    # we'll guess based on pathname and OS for now (mirrors old Imake tests)
    AC_MSG_CHECKING([man config file format])
    if test x$MANCONF = x/etc/manpath.config ; then
	MAN_CONFIG_STYLE="FreeBSD"
    else
	case $host_os in
	*darwin* | *openbsd* | *netbsd* )
	    MAN_CONFIG_STYLE="OpenBSD"
	    ;;
    	*linux* )
	    MAN_CONFIG_STYLE="Linux"
	    ;;
        *bsd* )
	    MAN_CONFIG_STYLE="BSD"
	    ;;
	*)
	    ;;
	esac
    fi
    AC_MSG_RESULT($MAN_CONFIG_STYLE)

    case $MAN_CONFIG_STYLE in
	FreeBSD)
	    AC_DEFINE([MANCONFIGSTYLE_FreeBSD],1,
		[Define to 1 if you have FreeBSD format manpath.config])
	    ;;
	OpenBSD)
	    AC_DEFINE([MANCONFIGSTYLE_OpenBSD],1,
		[Define to 1 if you have OpenBSD format manpath.config])
	    ;;
	BSD)
	    AC_DEFINE([MANCONFIGSTYLE_BSD],1,
		[Define to 1 if you have BSD format manpath.config])
	    ;;
	Linux)
	    AC_DEFINE([MANCONFIGSTYLE_Linux],1,
		[Define to 1 if you have Linux format man.conf or man.config])
	    ;;
	*)
	    AC_MSG_ERROR([Could not determine man page file config format.])
    esac
fi

if test "x$xaw_use_xprint" = "xyes" ; then
   PKG_CHECK_MODULES(XPRINT_UTIL, xprintutil xp)

   XMAN_CFLAGS="$XMAN_CFLAGS $XPRINT_UTIL_CFLAGS"
   XMAN_LIBS="$XMAN_LIBS $XPRINT_UTIL_LIBS"
fi

AC_SUBST(XMAN_CFLAGS)
AC_SUBST(XMAN_LIBS)

PKG_CHECK_MODULES(APPDEFS, xt)
appdefaultdir=$(pkg-config --variable=appdefaultdir xt)
AC_SUBST(appdefaultdir)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
