# 
#  $Id$
# 
#  Copyright © 2003 Keith Packard, Noah Levitt
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Keith Packard not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Keith Packard makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
# 
#  KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL COMPOSITES OR ANY COMPOSITES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

AM_CFLAGS = $(XCOMPOSITE_CFLAGS) $(XCOMPOSITE_CFLAGS) $(X_CFLAGS) -I$(top_srcdir)/include

lib_LTLIBRARIES = libXcomposite.la

libXcomposite_la_SOURCES = \
	xcompositeint.h	\
	Xcomposite.c

libXcomposite_la_LIBADD = $(XCOMPOSITE_LIBS) $(X_LIBS) $(XCOMPOSITE_LIBS)

libXcomposite_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXcompositeincludedir = $(includedir)/X11/extensions
libXcompositeinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xcomposite.h 
