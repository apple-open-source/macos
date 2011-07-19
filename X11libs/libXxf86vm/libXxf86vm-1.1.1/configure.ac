AC_PREREQ([2.60])

AC_INIT([libXxf86vm], [1.1.1],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [libXxf86vm])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS
AM_CONFIG_HEADER(src/config.h)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for pkg-config packages
PKG_CHECK_MODULES(XXF86VM, xproto x11 xextproto xext [xf86vidmodeproto >= 2.2.99.1])

XORG_CHECK_MALLOC_ZERO

AC_OUTPUT([Makefile
	   src/Makefile
	   man/Makefile
	   xxf86vm.pc])
