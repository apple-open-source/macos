lib_LTLIBRARIES = libXevie.la

libXevie_la_SOURCES = Xevie.c

libXevie_la_LIBADD = $(XEVIE_LIBS)

AM_CFLAGS = $(XEVIE_CFLAGS) $(MALLOC_ZERO_CFLAGS)

INCLUDES = -I$(top_srcdir)/include

libXevie_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXevieincludedir = $(includedir)/X11/extensions
libXevieinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xevie.h 
