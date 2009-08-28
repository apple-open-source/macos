#  Copyright 2005  Red Hat, Inc.
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Red Hat makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
# 
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

lib_LTLIBRARIES = libxrxnest.la

RXSRC = $(top_srcdir)/rx
PLUGINSRC = $(top_srcdir)/plugin

INCLUDES = -I$(RXSRC)

# -DPLUGIN_TRACE can be added to the following definition to debug
PLUGIN_DEFINES = -DXP_UNIX -DMOZ_X11 -DOJI

DEFINES = -DNETSCAPE_PLUGIN $(PLUGIN_DEFINES)

AM_CPPFLAGS = $(DEFINES)

PARAMS_SRCS = $(RXSRC)/PRead.c $(RXSRC)/PParse.c $(RXSRC)/BuildReq.c
XMISC_SRCS = $(RXSRC)/XUrls.c
PLUGIN_SRCS = $(PLUGINSRC)/Main.c $(PLUGINSRC)/stubs.c

libxrxnest_la_SOURCES = \
	NewNDest.c	\
	PProcess.c	\
	RxPlugin.h	\
	SetWin.c	\
	XnestDis.c	\
	$(PARAMS_SRCS)	\
	$(XMISC_SRCS)	\
	$(PLUGIN_SRCS)

AM_CFLAGS = @XRX_CFLAGS@ @XRX_PLUGIN_CFLAGS@ @PLUGIN_CFLAGS@

libxrxnest_la_LIBADD = @XRX_LIBS@ @XRX_PLUGIN_LIBS@
libxrxnest_la_LDFLAGS = -avoid-version
