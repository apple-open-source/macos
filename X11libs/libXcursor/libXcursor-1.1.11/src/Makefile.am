lib_LTLIBRARIES = libXcursor.la

libXcursor_la_SOURCES = xcursorint.h cursor.c display.c file.c library.c xlib.c

ICONDIR=@ICONDIR@
XCURSORPATH=@XCURSORPATH@

libXcursor_la_LIBADD = $(XCURSOR_LIBS)
AM_CFLAGS = \
	$(XCURSOR_CFLAGS) \
	$(CWARNFLAGS)\
	-DICONDIR=\"$(ICONDIR)\" \
	-DXCURSORPATH=\"$(XCURSORPATH)\"

INCLUDES = -I$(top_srcdir)/include/X11/Xcursor

#
# Shared library version info.  This is not the same as the package version
#
libXcursor_la_LDFLAGS = -version-number 1:0:2 -no-undefined

libXcursorincludedir = $(includedir)/X11/Xcursor
libXcursorinclude_HEADERS = $(top_srcdir)/include/X11/Xcursor/Xcursor.h 

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(libXcursor_la_SOURCES) $(XCURSOR_LIBS)
endif LINT
