#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([libXext],
        1.0.2,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        libXext)

AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

AM_CONFIG_HEADER([config.h])

# Determine .so library version per platform 
# based on SharedXextRev in monolith xc/config/cf/*Lib.tmpl
AC_CANONICAL_HOST
case $host_os in
	openbsd*)	XEXT_SOREV=8:0 		;;
	solaris*)       XEXT_SOREV=0 		;;
	*)              XEXT_SOREV=6:4:0 	;;
esac
AC_SUBST(XEXT_SOREV)

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(XEXT, xproto x11 xextproto xau)
AC_SUBST(XEXT_CFLAGS)
AC_SUBST(XEXT_LIBS)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

XORG_CHECK_MALLOC_ZERO
XORG_RELEASE_VERSION

# Man page processing requirements
XORG_PROG_RAWCPP
XORG_MANPAGE_SECTIONS

AC_OUTPUT([Makefile
           man/Makefile
	   src/Makefile
	   xext.pc])
