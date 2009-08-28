
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-property.la

xcbinclude_HEADERS = xcb_property.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_property_la_SOURCES = property.c
libxcb_property_la_CPPFLAGS = $(XCB_CFLAGS) $(XCB_EVENT_CFLAGS)
libxcb_property_la_LIBADD = $(XCB_LIBS) $(XCB_EVENT_LIBS)
libxcb_property_la_LDFLAGS = -version-info 1:0:0

pkgconfig_DATA = xcb-property.pc

EXTRA_DIST = xcb-property.pc.in
