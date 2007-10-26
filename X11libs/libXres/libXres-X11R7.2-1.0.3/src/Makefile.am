lib_LTLIBRARIES = libXRes.la

libXRes_la_SOURCES =    \
        XRes.c

libXRes_la_LIBADD = @RESOURCEEXT_LIBS@ @XRES_LIBS@

AM_CFLAGS = @RESOURCEEXT_CFLAGS@ @XRES_CFLAGS@ @MALLOC_ZERO_CFLAGS@

INCLUDES = -I$(top_srcdir)/include

libXRes_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXResincludedir = $(includedir)/X11/extensions
libXResinclude_HEADERS = $(top_srcdir)/include/X11/extensions/XRes.h

