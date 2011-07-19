#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([libICE],
        [1.0.7],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [libICE])

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AM_MAINTAINER_MODE
AM_CONFIG_HEADER([config.h])

# Require xorg-macros minimum of 1.10 for DocBook XML documentation
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.10 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.10)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_DOCS
XORG_ENABLE_SPECS
XORG_WITH_XMLTO(0.0.20)
XORG_WITH_FOP
XORG_CHECK_SGML_DOCTOOLS(1.5)

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(ICE, xproto)

PKG_CHECK_MODULES(XTRANS, xtrans)
AC_SUBST(XTRANS_CFLAGS)
AC_SUBST(XTRANS_LIBS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS
AC_DEFINE(ICE_t, 1, [Xtrans transport type])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([ICE])
LINT_FLAGS="${LINT_FLAGS} ${ICE_CFLAGS} ${XTRANS_CFLAGS}"


AC_OUTPUT([Makefile
	   doc/Makefile
	   specs/Makefile
	   src/Makefile
	   ice.pc])
