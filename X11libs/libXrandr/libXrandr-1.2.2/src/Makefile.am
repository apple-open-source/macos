lib_LTLIBRARIES = libXrandr.la

libXrandr_la_SOURCES = 	\
	Xrandr.c 	\
	Xrandrint.h	\
	XrrConfig.c	\
	XrrCrtc.c	\
	XrrMode.c	\
	XrrOutput.c	\
	XrrProperty.c	\
	XrrScreen.c

libXrandr_la_LIBADD = @X_LIBS@ @RANDR_LIBS@
AM_CFLAGS = @X_CFLAGS@ @RANDR_CFLAGS@ @MALLOC_ZERO_CFLAGS@

INCLUDES = -I$(top_srcdir)/include/X11/extensions

libXrandr_la_LDFLAGS = -version-number 2:1:0 -no-undefined

libXrandrincludedir = $(includedir)/X11/extensions
libXrandrinclude_HEADERS = $(top_srcdir)/include/X11/extensions/Xrandr.h 
