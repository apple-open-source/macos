dnl  Copyright 2007 Peter Hutterer <peter@cs.unisa.edu.au>
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
AC_INIT(xinput,[1.5.3], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xinput)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require xorg-macros: XORG_DEFAULT_OPTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.3 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.3)
XORG_DEFAULT_OPTIONS

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED


# Checks for pkg-config packages
PKG_CHECK_MODULES(XINPUT, x11 xext [xi >= 1.2] [inputproto >= 1.5])

# XI2 support
PKG_CHECK_MODULES(XI2, [xi >= 1.2.99.2] [inputproto >= 1.9.99.15],
                  HAVE_XI2="yes"; AC_DEFINE(HAVE_XI2, 1, [XI2 available]),
                  HAVE_XI2="no");
AM_CONDITIONAL(HAVE_XI2, [ test "$HAVE_XI2" = "yes" ])

AC_SUBST(XINPUT_CFLAGS)
AC_SUBST(XINPUT_LIBS)
AC_SUBST(HAVE_XI2)

AC_SUBST(VERSION)

AC_OUTPUT([Makefile
           src/Makefile
           man/Makefile])
