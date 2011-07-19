#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([libSM],
        [1.2.0],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libSM)

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AM_MAINTAINER_MODE

# Require xorg-macros minimum of 1.10 for DocBook XML documentation
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.10 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.10)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_DOCS
XORG_WITH_XMLTO(0.0.20)
XORG_WITH_FOP
XORG_CHECK_SGML_DOCTOOLS(1.5)

AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(SM, [ice >= 1.0.5] xproto)
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

AC_CHECK_FUNCS([uuid_create], [], [
    if test x"$with_libuuid" != xno && test x"$have_system_uuid" != xyes; then
        PKG_CHECK_MODULES(LIBUUID, uuid, [HAVE_LIBUUID=yes], [HAVE_LIBUUID=no])
    fi
    if test x"$with_libuuid" = xyes && test x"$HAVE_LIBUUID" = xno; then
        AC_MSG_ERROR([requested libuuid support but uuid.pc not found])
    fi
])

AM_CONDITIONAL(WITH_LIBUUID, test x"$HAVE_LIBUUID" = xyes)

AC_OUTPUT([Makefile
	   doc/Makefile
	   src/Makefile
	   sm.pc])
