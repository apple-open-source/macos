dnl $XdotOrg: lib/Xi/configure.ac,v 1.9 2005/12/15 00:24:30 kem Exp $
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])

AC_INIT(libXi, 1.2.1, [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], libXi)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(src/config.h)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for pkg-config packages
PKG_CHECK_MODULES(XI, xproto x11 xextproto xext inputproto >= 1.5)
AC_SUBST(XI_CFLAGS)
AC_SUBST(XI_LIBS)

XORG_CHECK_MALLOC_ZERO
XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
	   src/Makefile
	   man/Makefile
           xi.pc])

