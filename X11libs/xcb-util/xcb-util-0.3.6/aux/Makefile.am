
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-aux.la

xcbinclude_HEADERS = xcb_aux.h xcb_bitops.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_aux_la_SOURCES = xcb_aux.c
libxcb_aux_la_CPPFLAGS = $(XCB_CFLAGS)
libxcb_aux_la_LIBADD = $(XCB_LIBS)

pkgconfig_DATA = xcb-aux.pc

EXTRA_DIST=xcb-aux.pc.in
