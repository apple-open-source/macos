lib_LTLIBRARIES = libdmx.la

libdmx_la_SOURCES = \
         dmx.c

libdmx_la_LIBADD = $(DMX_LIBS)

libdmx_la_LDFLAGS = -version-number 1:0:0 -no-undefined

AM_CFLAGS = -I$(top_srcdir)/include \
            -I$(top_srcdir)/include/X11 \
            -I$(top_srcdir)/include/X11/extensions \
            $(DMX_CFLAGS) \
            $(MALLOC_ZERO_CFLAGS) \
            $(CWARNFLAGS)

libdmxincludedir = $(includedir)/X11/extensions
libdmxinclude_HEADERS = $(top_srcdir)/include/X11/extensions/dmxext.h

