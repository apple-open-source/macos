# $Id$

AC_PREREQ(2.57)
AC_INIT([libXpm], 3.5.7, [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], libXpm)
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(XPM, xproto x11)
AC_SUBST(XPM_CFLAGS)
AC_SUBST(XPM_LIBS)

# Checks for header files.
AC_HEADER_STDC

# Checks for functions
AC_CHECK_FUNCS([strlcat])

# Checks for typedefs, structures, and compiler characteristics.

if test "x$GCC" = "xyes"; then
	GCC_WARNINGS="-Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs -fno-strict-aliasing"
	CFLAGS="$GCC_WARNINGS $CFLAGS"
fi

# Internationalization & localization support
AC_SEARCH_LIBS([gettext], [intl], [USE_GETTEXT="yes"], [USE_GETTEXT="no"])
AC_MSG_CHECKING([where to install localized messages])
AC_ARG_WITH([localedir], AC_HELP_STRING([--with-localedir=<path>],
	[Path to install message files in (default: datadir/locale)]),
	[LOCALEDIR=${withval}], [LOCALEDIR=${datadir}/locale])
AC_DEFINE_DIR([LOCALEDIR], [LOCALEDIR], [Location of translated messages])
if test "x$LOCALEDIR" = "xno" -o "x$USE_GETTEXT" = "xno" ; then
	AC_MSG_RESULT([nowhere])
	USE_GETTEXT="no"
else
	AC_MSG_RESULT([$LOCALEDIR])
fi

if test "x$USE_GETTEXT" = "xyes" ; then
	AC_DEFINE([USE_GETTEXT], 1, 
		  [Define to 1 if you want to use the gettext() function.])
fi
AM_CONDITIONAL(USE_GETTEXT, test "x$USE_GETTEXT" = "xyes")

# Optional feature: When ___.xpm is requested, also look for ___.xpm.Z & .gz
# Replaces ZFILEDEF = -DSTAT_ZFILE in old Imakefile
AC_ARG_ENABLE(stat-zfile,
	AC_HELP_STRING([--enable-stat-zfile],
			[Search for files with .Z & .gz extensions automatically @<:@default=yes@:>@]),
              [STAT_ZFILE=$enableval], [STAT_ZFILE=yes])
if test x$STAT_ZFILE = xyes ; then
	AC_DEFINE(STAT_ZFILE, 1, [Define to 1 to automatically look for files with .Z & .gz extensions])
fi

PKG_CHECK_MODULES(SXPM, xt xext xextproto, build_sxpm=true, build_sxpm=false)

AM_CONDITIONAL(BUILD_SXPM, test x$build_sxpm = xtrue)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
           src/Makefile
	   sxpm/Makefile
	   cxpm/Makefile
	   xpm.pc])
