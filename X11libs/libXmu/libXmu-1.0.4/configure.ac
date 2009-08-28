
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)

AC_INIT([libXmu],
        1.0.4,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libXmu)

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

# Require xorg-macros version 1.1.0 or newer for XORG_WITH_LINT macro
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.1 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.1)


# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(XMU, xt xext xextproto)
AC_SUBST(XMU_CFLAGS)
AC_SUBST(XMU_LIBS)

PKG_CHECK_MODULES(XMUU, x11)
AC_SUBST(XMUU_CFLAGS)
AC_SUBST(XMUU_LIBS)

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

if test "x$GCC" = "xyes"; then
        GCC_WARNINGS="-Wall -Wpointer-arith -Wstrict-prototypes \
        -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
        XMU_CFLAGS="$GCC_WARNINGS $XMU_CFLAGS"
fi

XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
           include/Makefile
	   src/Makefile
	   xmu.pc
           xmuu.pc])
