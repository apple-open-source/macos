lib_LTLIBRARIES = libXprintUtil.la

libXprintUtil_la_SOURCES = \
         xprintutil.c \
         xprintutil_printtofile.c

INCLUDES = -I$(top_srcdir)/include/X11/XprintUtil


libXprintUtil_la_LIBADD = @XPRINTUTIL_LIBS@
AM_CFLAGS = @XPRINTUTIL_CFLAGS@

libXprintUtil_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXprintUtilincludedir = $(includedir)/X11/XprintUtil
libXprintUtilinclude_HEADERS = \
    $(top_srcdir)/include/X11/XprintUtil/xprintutil.h
