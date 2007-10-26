lib_LTLIBRARIES = liboldX.la

liboldX_la_SOURCES = \
         XCrAssoc.c \
         XDelAssoc.c \
         XDestAssoc.c \
         XDraw.c \
         XLookAssoc.c \
         XMakeAssoc.c

INCLUDES = -I$(top_srcdir)/include/X11


liboldX_la_LIBADD = @OLDX_LIBS@
AM_CFLAGS = @OLDX_CFLAGS@ @MALLOC_ZERO_CFLAGS@

liboldX_la_LDFLAGS = -version-number 6:0:0 -no-undefined

liboldXincludedir = $(includedir)/X11
liboldXinclude_HEADERS = \
    $(top_srcdir)/include/X11/X10.h

