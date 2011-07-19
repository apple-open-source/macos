AC_PREREQ([2.57])
AC_INIT([XF86DGAProto], [2.1], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

# Require xorg-macros: XORG_DEFAULT_OPTIONS
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.3 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.3)

XORG_DEFAULT_OPTIONS

AC_OUTPUT([Makefile
           xf86dgaproto.pc])
