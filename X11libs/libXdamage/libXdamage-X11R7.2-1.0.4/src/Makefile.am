lib_LTLIBRARIES = libXdamage.la

libXdamage_la_SOURCES = \
	xdamageint.h	\
	Xdamage.c

libXdamage_la_LIBADD = $(XDAMAGE_LIBS) $(X_LIBS)
AM_CFLAGS = $(XDAMAGE_CFLAGS) $(X_CFLAGS)

INCLUDES = -I$(top_srcdir)/include/X11/extensions

libXdamage_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libXdamageincludedir = $(includedir)/X11/extensions
libXdamageinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xdamage.h 
