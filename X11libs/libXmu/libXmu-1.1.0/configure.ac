
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])

AC_INIT([libXmu],
        [1.1.0],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [libXmu])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AM_MAINTAINER_MODE

# Require xorg-macros minimum of 1.10 for HAVE_STYLESHEETS in XORG_CHECK_SGML_DOCTOOLS
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

# Checks for pkg-config packages
PKG_CHECK_MODULES(XMU, xt xext x11 xextproto)
PKG_CHECK_MODULES(XMUU, x11)

# CvtStdSel.c needs to know which network transports to build hostname
# conversion routines for
XTRANS_CONNECTION_FLAGS

#
# Assume that we have sprintf. If a system shows up
# that doesn't, we will deal with it at that point.
#
# The way to deal with it is:
#
#  1. Add a test for sprintf it here in configure.ac
#     that sets or unsets HAS_SNPRINTF as appropriate
#
#  2. add a copy of xc/lib/misc/snprintf.c
#
# The source file in question is Lower.c
#

AC_DEFINE(HAS_SNPRINTF, [], [Whether we have snprintf()])
AC_SUBST(HAS_SNPRINTF)

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([Xmu])
LINTLIBUU=`echo $LINTLIB | sed s/Xmu/Xmuu/`
AC_SUBST(LINTLIBUU)

AC_OUTPUT([Makefile
           doc/Makefile
           include/Makefile
	   src/Makefile
	   xmu.pc
           xmuu.pc])
