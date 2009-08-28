# $XdotOrg: app/xhost/configure.ac,v 1.10 2005/12/15 00:24:07 kem Exp $

AC_PREREQ([2.57])
AC_INIT(xhost,[1.0.2], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],xhost)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

AC_TYPE_SIGNAL

# Checks for pkg-config packages
PKG_CHECK_MODULES(XHOST, x11 xmuu xau)
AC_SUBST(XHOST_CFLAGS)
AC_SUBST(XHOST_LIBS)

# Transport selection macro from xtrans.m4
XTRANS_CONNECTION_FLAGS

# Secure RPC detection macro from xtrans.m4
XTRANS_SECURE_RPC_FLAGS

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
