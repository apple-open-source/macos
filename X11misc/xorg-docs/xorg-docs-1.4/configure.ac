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

AC_PREREQ([2.57])
AC_INIT([xorg-docs], [1.4], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

DEFAULT_X11DOCDIR=${datadir}/X11/doc
AC_ARG_WITH(x11docdir,
        AC_HELP_STRING([--with-x11docdir=<path>],
                       [Where to install X11 docs (default: ${datadir}/X11/doc)]),
        [X11DOCDIR="$withval"],
        [X11DOCDIR="$DEFAULT_X11DOCDIR"])
HARDCOPYDIR=$X11DOCDIR/hardcopy
AC_SUBST([X11DOCDIR])
AC_SUBST([HARDCOPYDIR])

AC_ARG_ENABLE(txt, AS_HELP_STRING([--enable-txt],
                [Build TXT versions of documents (default: enabled)]),
                [BUILD_TXTDOC=$enableval], [BUILD_TXTDOC=yes])
AC_ARG_ENABLE(pdf, AS_HELP_STRING([--enable-pdf],
                [Build PDF versions of documents (default: enabled)]),
                [BUILD_PDFDOC=$enableval], [BUILD_PDFDOC=yes])
AC_ARG_ENABLE(ps, AS_HELP_STRING([--enable-ps],
                [Build PostScript versions of documents (default: disabled)]),
                [BUILD_PSDOC=$enableval], [BUILD_PSDOC=no])
AC_ARG_ENABLE(html, AS_HELP_STRING([--enable-html],
                [Build HTML versions of documents (default: disabled)]),
                [BUILD_HTMLDOC=$enableval], [BUILD_HTMLDOC=no])

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AM_CONDITIONAL(BUILD_TXTDOC, test "x$BUILD_TXTDOC" = xyes)
AM_CONDITIONAL(BUILD_PDFDOC, test "x$BUILD_PDFDOC" = xyes)
AM_CONDITIONAL(BUILD_PSDOC, test "x$BUILD_PSDOC" = xyes)
AM_CONDITIONAL(BUILD_HTMLDOC, test "x$BUILD_HTMLDOC" = xyes)
XORG_CHECK_DOCBOOK

AC_OUTPUT([Makefile
           hardcopy/Makefile
           hardcopy/BDF/Makefile
           hardcopy/CTEXT/Makefile
           hardcopy/FSProtocol/Makefile
           hardcopy/i18n/Makefile
           hardcopy/ICCCM/Makefile
           hardcopy/ICE/Makefile
           hardcopy/man/Makefile
           hardcopy/rstart/Makefile
           hardcopy/RX/Makefile
           hardcopy/saver/Makefile
           hardcopy/SM/Makefile
           hardcopy/X11/Makefile
           hardcopy/Xaw/Makefile
           hardcopy/XDMCP/Makefile
           hardcopy/Xext/Makefile
           hardcopy/xfs/Makefile
           hardcopy/Xi/Makefile
           hardcopy/XIM/Makefile
           hardcopy/XKB/Makefile
           hardcopy/XLFD/Makefile
           hardcopy/Xmu/Makefile
           hardcopy/XPRINT/Makefile
           hardcopy/XProtocol/Makefile
           hardcopy/Xserver/Makefile
           hardcopy/Xt/Makefile
           hardcopy/xtrans/Makefile
           hardcopy/Xv/Makefile
           man/Makefile
           man/general/Makefile
           sgml/Makefile
	   sgml/core/Makefile
	   sgml/ddx/Makefile
	   sgml/fonts/Makefile
	   sgml/graphics/Makefile
	   sgml/input/Makefile
	   sgml/output/Makefile
	   sgml/platforms/Makefile
	   sgml/registry/Makefile
	   sgml/security/Makefile])
