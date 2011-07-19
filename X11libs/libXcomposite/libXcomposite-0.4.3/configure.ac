dnl 
dnl  Copyright © 2003 Keith Packard, Noah Levitt
dnl 
dnl  Permission to use, copy, modify, distribute, and sell this software and its
dnl  documentation for any purpose is hereby granted without fee, provided that
dnl  the above copyright notice appear in all copies and that both that
dnl  copyright notice and this permission notice appear in supporting
dnl  documentation, and that the name of Keith Packard not be used in
dnl  advertising or publicity pertaining to distribution of the software without
dnl  specific, written prior permission.  Keith Packard makes no
dnl  representations about the suitability of this software for any purpose.  It
dnl  is provided "as is" without express or implied warranty.
dnl 
dnl  KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl  EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl  PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.60])

dnl
dnl Version should match the current XComposite version. XCompositeQueryVersion
dnl returns the version from xcompositewire.h, NOT the version we set here.
dnl But we try to keep these the same.  Note that the library has an extra
dnl digit in the version number to track changes which don't affect the
dnl protocol, so Xcomposite version l.n.m corresponds to protocol version l.n
dnl that 'revision' number appears in Xcomposite.h and has to be manually
dnl synchronized.
dnl
AC_INIT(libXcomposite,
	[0.4.3],
	[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
	libXcomposite)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

# Require X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION],
	  [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
AM_CONFIG_HEADER(config.h)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

XORG_DEFAULT_OPTIONS
XORG_WITH_XMLTO(0.0.20)

# Determine if the source for man pages is available
# It may already be present (tarball) or can be generated using xmlto
AM_CONDITIONAL([INSTALL_MANPAGES],
	[test -f "$srcdir/man/Xcomposite.man" || test "x$have_xmlto" = xyes])

# Check compositeext configuration, strip extra digits from package version to
# find the required protocol version

if test "$VERSION" = "" ; then
       VERSION=$PACKAGE_VERSION;
fi
COMPOSITEEXT_VERSION=[`echo $VERSION | sed 's/^\([0-9][0-9]*\.[0-9][0-9]*\).*$/\1/'`]
AC_SUBST(COMPOSITEEXT_VERSION)
PKG_CHECK_MODULES(XCOMPOSITE, [compositeproto >= $COMPOSITEEXT_VERSION] x11)
PKG_CHECK_MODULES(XFIXES, xfixes)

AC_OUTPUT([Makefile
	   src/Makefile
	   man/Makefile
           xcomposite.pc])

