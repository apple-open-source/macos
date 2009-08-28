#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([libSM],
        1.1.0,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libSM)

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(SM, ice xproto)
AC_SUBST(SM_CFLAGS)
AC_SUBST(SM_LIBS)

PKG_CHECK_MODULES(XTRANS, xtrans)
AC_SUBST(XTRANS_CFLAGS)
AC_SUBST(XTRANS_LIBS)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

# Needed to check for TCP & IPv6 support and set flags appropriately
XTRANS_CONNECTION_FLAGS

AC_ARG_WITH(libuuid, AC_HELP_STRING([--with-libuuid], [Build with libuuid support for client IDs]))
AM_CONDITIONAL(WITH_LIBUUID, test x"$with_libuuid" != xno)

if test x"$with_libuuid" != xno; then
	PKG_CHECK_MODULES(LIBUUID, uuid)
fi

if test "x$GCC" = "xyes"; then
	GCC_WARNINGS="-Wall -Wpointer-arith -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations \
	-Wnested-externs -fno-strict-aliasing"
	CFLAGS="$GCC_WARNINGS $CFLAGS"
fi

XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
	   src/Makefile
	   sm.pc])
