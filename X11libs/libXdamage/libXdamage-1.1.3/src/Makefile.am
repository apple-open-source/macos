lib_LTLIBRARIES = libXdamage.la

libXdamage_la_SOURCES = \
	xdamageint.h	\
	Xdamage.c

libXdamage_la_LIBADD = $(XDAMAGE_LIBS)
AM_CFLAGS = $(CWARNFLAGS) $(XDAMAGE_CFLAGS)

INCLUDES = -I$(top_srcdir)/include/X11/extensions

libXdamage_la_LDFLAGS = -version-number 1:1:0 -no-undefined

libXdamageincludedir = $(includedir)/X11/extensions
libXdamageinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xdamage.h 
