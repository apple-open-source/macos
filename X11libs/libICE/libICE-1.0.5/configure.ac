#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([libICE],
        1.0.5,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libICE)

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(ICE, xproto)
AC_SUBST(ICE_CFLAGS)
AC_SUBST(ICE_LIBS)

PKG_CHECK_MODULES(XTRANS, xtrans)
AC_SUBST(XTRANS_CFLAGS)
AC_SUBST(XTRANS_LIBS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS
AC_DEFINE(ICE_t, 1, [Xtrans transport type])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

# Require xorg-macros at least 1.2.1
XORG_MACROS_VERSION([1.2.1])
m4_ifndef([XORG_MACROS_VERSION],
	[AC_FATAL([must install xorg-macros 1.2.1 or later before running autoconf/autogen])])

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([ICE])
LINT_FLAGS="${LINT_FLAGS} ${ICE_CFLAGS} ${XTRANS_CFLAGS}"

XORG_CHANGELOG

XORG_CWARNFLAGS

XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
	   src/Makefile
	   ice.pc])
