
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-keysyms.la

xcbinclude_HEADERS = xcb_keysyms.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_keysyms_la_SOURCES = keysyms.c
libxcb_keysyms_la_CPPFLAGS = $(XCB_CFLAGS)
libxcb_keysyms_la_LIBADD = $(XCB_LIBS)
libxcb_keysyms_la_LDFLAGS = -version-info 1:0:0

pkgconfig_DATA = xcb-keysyms.pc

EXTRA_DIST=xcb-keysyms.pc.in
