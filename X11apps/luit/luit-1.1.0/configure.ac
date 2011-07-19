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
AC_INIT([luit], [1.1.0],
	[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], [luit])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST


AC_CHECK_HEADERS([pty.h stropts.h sys/param.h sys/select.h])
AC_CHECK_FUNCS([select grantpt])

AC_CHECK_LIB(z, gzopen,, AC_MSG_ERROR([zlib not found]))

AC_ARG_WITH(localealiasfile,
        AC_HELP_STRING([--with-localealiasfile=<path>],
                       [The locale alias file (default: ${datadir}/X11/locale/locale.alias)]),
        [LOCALEALIASFILE="$withval"],
        [LOCALEALIASFILE=${datadir}/X11/locale/locale.alias])
AC_SUBST([LOCALEALIASFILE])

# Checks for pkg-config packages
PKG_CHECK_MODULES(LUIT, fontenc)

# We don't link against libX11, just use its locale.alias data file
PKG_CHECK_EXISTS(x11, [],
	[AC_MSG_WARN([libX11 not found.  luit may not be able to find locale aliases without it.])])

case $host_os in
    # darwin has poll() but can't be used to poll character devices (atleast through SnowLeopard)
    darwin*)
        OS_CFLAGS=
        ;;
    linux*)
        AC_CHECK_HEADERS([poll.h])
        AC_CHECK_FUNCS([poll])
        OS_CFLAGS="-D_XOPEN_SOURCE=500"
        ;;
    *)
        AC_CHECK_HEADERS([poll.h])
        AC_CHECK_FUNCS([poll])
        OS_CFLAGS=
        ;;
esac
AC_SUBST(OS_CFLAGS)

AC_CHECK_HEADERS([pty.h stropts.h sys/ioctl.h sys/param.h sys/poll.h sys/select.h sys/time.h termios.h]) 
AC_CHECK_FUNCS([grantpt putenv select strdup])

if test "x$ac_cv_func_grantpt" != "xyes" ; then
        AC_CHECK_LIB(util, openpty, [cf_have_openpty=yes],[cf_have_openpty=no])
        if test "$cf_have_openpty" = yes ; then
                LIBS="-lutil $LIBS"
                AC_DEFINE(HAVE_OPENPTY, 1, [Have openpty in libutil])
                AC_CHECK_HEADERS( \
                        util.h \
                        libutil.h \
                        pty.h \
                )
        fi
fi

CF_FUNC_POLL
CF_SIGWINCH

AC_CONFIG_FILES([Makefile
                 man/Makefile])

AC_OUTPUT
