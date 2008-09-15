dnl  Copyright 2007 Peter Hutterer <peter@cs.unisa.edu.au>
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
AC_INIT(xinput,[1.3.0], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xinput)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

# Checks for pkg-config packages
PKG_CHECK_MODULES(XINPUT, x11 xext xi inputproto >= 1.4)
AC_SUBST(XINPUT_CFLAGS)
AC_SUBST(XINPUT_LIBS)

AC_SUBST(VERSION)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
           src/Makefile
           man/Makefile])
