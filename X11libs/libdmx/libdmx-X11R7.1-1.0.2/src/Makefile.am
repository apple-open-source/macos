lib_LTLIBRARIES = libdmx.la

libdmx_la_SOURCES = \
         dmx.c

libdmx_la_LIBADD = $(DMX_LIBS)

libdmx_la_LDFLAGS = -version-number 1:0:0 -no-undefined

AM_CFLAGS = $(DMX_CFLAGS) $(MALLOC_ZERO_CFLAGS)
