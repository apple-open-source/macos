lib_LTLIBRARIES = libXprintAppUtil.la

libXprintAppUtil_la_SOURCES = \
         xpapputil.c

INCLUDES = -I$(top_srcdir)/include


libXprintAppUtil_la_LIBADD = @XPRINTAPPUTIL_LIBS@
AM_CFLAGS = @XPRINTAPPUTIL_CFLAGS@

libXprintAppUtil_la_LDFLAGS = -version-number 1:0:0 -no-undefined


libXprintAppUtilincludedir = $(includedir)/X11/XprintAppUtil
libXprintAppUtilinclude_HEADERS = \
    $(top_srcdir)/include/X11/XprintAppUtil/xpapputil.h
