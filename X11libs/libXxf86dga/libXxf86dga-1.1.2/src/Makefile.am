lib_LTLIBRARIES = libXxf86dga.la

libXxf86dga_la_SOURCES = XF86DGA.c XF86DGA2.c

AM_CFLAGS = -I $(top_srcdir)/include \
            $(XXF86DGA_CFLAGS) \
            $(MALLOC_ZERO_CFLAGS) \
            $(CWARNFLAGS)
           
libXxf86dga_la_LIBADD = $(XXF86DGA_LIBS)
libXxf86dga_la_LDFLAGS = -version-number 1:0:0

libXxf86dgaincludedir = $(includedir)/X11/extensions
libXxf86dgainclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xxf86dga.h \
                             $(top_srcdir)/include/X11/extensions/xf86dga1.h
