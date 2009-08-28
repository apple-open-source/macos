#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([libXext],
        1.0.5,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libXext)

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

# Require xorg-macros: XORG_CWARNFLAGS, XORG_CHANGELOG, XORG_WITH_LINT
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.2 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.2)
AM_CONFIG_HEADER([config.h])

# Determine .so library version per platform 
# based on SharedXextRev in monolith xc/config/cf/*Lib.tmpl
AC_CANONICAL_HOST
if test "x$XEXT_SOREV" = "x" ; then
    case $host_os in
	openbsd*)	XEXT_SOREV=8:0 		;;
	solaris*)       XEXT_SOREV=0 		;;
	*)              XEXT_SOREV=6:4:0 	;;
    esac
fi
AC_SUBST(XEXT_SOREV)

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
XORG_CWARNFLAGS

# Checks for pkg-config packages
PKG_CHECK_MODULES(XEXT, [xproto >= 7.0.13] [x11 >= 1.1.99.1] [xextproto >= 7.0.5] xau)
XEXT_CFLAGS="$CWARNFLAGS $XEXT_CFLAGS"
AC_SUBST(XEXT_CFLAGS)
AC_SUBST(XEXT_LIBS)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

XORG_CHECK_MALLOC_ZERO
XORG_RELEASE_VERSION
XORG_CHANGELOG

# Man page processing requirements
XORG_PROG_RAWCPP
XORG_MANPAGE_SECTIONS

dnl Allow checking code with lint, sparse, etc.
XORG_WITH_LINT
XORG_LINT_LIBRARY([Xext])
LINT_FLAGS="${LINT_FLAGS} ${XEXT_CFLAGS}"

AC_OUTPUT([Makefile
           man/Makefile
	   src/Makefile
	   xext.pc])
