
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-reply.la

xcbinclude_HEADERS = xcb_reply.h

AM_CFLAGS = $(CWARNFLAGS)

XCB_REPLY_LIBS = libxcb-reply.la

libxcb_reply_la_SOURCES = reply.c
libxcb_reply_la_CPPFLAGS = $(XCB_CFLAGS)
libxcb_reply_la_LIBADD = $(XCB_LIBS) -lpthread
libxcb_reply_la_LDFLAGS = -version-info 1:0:0

pkgconfig_DATA = xcb-reply.pc

EXTRA_DIST=xcb-reply.pc.in

noinst_PROGRAMS = test_reply

test_reply_CPPFLAGS = $(XCB_CFLAGS)
test_reply_LDADD = $(XCB_LIBS) $(XCB_REPLY_LIBS)
test_reply_SOURCES = test_reply.c
