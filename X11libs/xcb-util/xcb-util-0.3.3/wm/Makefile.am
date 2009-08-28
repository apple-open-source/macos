
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-wm.la

xcbinclude_HEADERS = xcb_wm.h

AM_CFLAGS = $(CWARNFLAGS)

XCB_XCBWM_LIBS = libxcb-wm.la

libxcb_wm_la_SOURCES = manage.c table.c
libxcb_wm_la_CPPFLAGS = $(XCB_CFLAGS) \
	$(XCB_AUX_CFLAGS) \
	$(XCB_ATOM_CFLAGS) \
	$(XCB_EVENT_CFLAGS) \
	$(XCB_PROPERTY_CFLAGS) \
	$(XCB_ICCCM_CFLAGS)
libxcb_wm_la_LIBADD = $(XCB_LIBS) \
	$(XCB_AUX_LIBS) \
	$(XCB_ATOM_LIBS) \
	$(XCB_EVENT_LIBS) \
	$(XCB_PROPERTY_LIBS) \
	$(XCB_ICCCM_LIBS)

pkgconfig_DATA = xcb-wm.pc

EXTRA_DIST = xcb-wm.pc.in

noinst_PROGRAMS = xcbwm-test

xcbwm_test_SOURCES = xcbwm-test.c reply_formats.c reply_formats.h
xcbwm_test_CPPFLAGS = $(XCB_CFLAGS) \
	$(XCB_AUX_CFLAGS) \
	$(XCB_ATOM_CFLAGS) \
	$(XCB_EVENT_CFLAGS) \
	$(XCB_PROPERTY_CFLAGS) \
	$(XCB_ICCCM_CFLAGS)
xcbwm_test_LDADD = $(XCB_LIBS) \
	$(XCB_ATOM_LIBS) \
	$(XCB_AUX_LIBS) \
	$(XCB_EVENT_LIBS) \
	$(XCB_PROPERTY_LIBS) \
	$(XCB_ICCCM_LIBS) \
	$(XCB_XCBWM_LIBS) \
	-lpthread
