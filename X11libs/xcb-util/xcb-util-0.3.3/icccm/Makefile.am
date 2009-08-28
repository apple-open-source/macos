
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-icccm.la

xcbinclude_HEADERS = xcb_icccm.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_icccm_la_SOURCES = icccm.c
libxcb_icccm_la_CPPFLAGS = $(XCB_CFLAGS) $(XCB_ATOM_CFLAGS) $(XCB_EVENT_CFLAGS) \
	$(XCB_PROPERTY_CFLAGS)
libxcb_icccm_la_LIBADD = $(XCB_LIBS) $(XCB_ATOM_LIBS) $(XCB_PROPERTY_LIBS)
libxcb_icccm_la_LDFLAGS = -version-info 1:0:0

pkgconfig_DATA = xcb-icccm.pc

EXTRA_DIST = xcb-icccm.pc.in
