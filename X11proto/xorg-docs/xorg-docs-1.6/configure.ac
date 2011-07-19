dnl  Copyright 2005 Red Hat, Inc.
dnl 
dnl  Permission to use, copy, modify, distribute, and sell this software
dnl  and its documentation for any purpose is hereby granted without fee,
dnl  provided that the above copyright notice appear in all copies and
dnl  that both that copyright notice and this permission notice appear in
dnl  supporting documentation, and that the name of Red Hat not be used in
dnl  advertising or publicity pertaining to distribution of the software
dnl  without specific, written prior permission.  Red Hat makes no
dnl  representations about the suitability of this software for any
dnl  purpose.  It is provided "as is" without express or implied warranty.
dnl 
dnl  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
dnl  NO EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
dnl  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
dnl  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
dnl  USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl  Process this file with autoconf to create configure.

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([xorg-docs],
        [1.6],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [xorg-docs])
AC_CONFIG_SRCDIR([Makefile.am])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

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

AC_CONFIG_FILES([Makefile
		 general/Makefile
		 general/fonts/Makefile
		 general/input/Makefile
		 general/graphics/Makefile
		 general/platforms/Makefile
		 man/Makefile
		 specs/Makefile
		 specs/CTEXT/Makefile
		 specs/ICCCM/Makefile
		 specs/Xext/Makefile
		 specs/XLFD/Makefile
		 specs/Xserver/Makefile])
AC_OUTPUT
