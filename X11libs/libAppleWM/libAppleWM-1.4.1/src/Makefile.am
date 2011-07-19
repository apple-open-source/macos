lib_LTLIBRARIES = libAppleWM.la

libAppleWM_la_SOURCES = \
	applewm.c

AM_CFLAGS = \
	$(APPLEWM_CFLAGS) \
	$(MALLOC_ZERO_CFLAGS) \
	$(CWARNFLAGS) \
	-F/System/Library/Frameworks/ApplicationServices.framework/Frameworks \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/include/X11 \
	-I$(top_srcdir)/include/X11/extensions

libAppleWM_la_LIBADD = @APPLEWM_LIBS@

libAppleWM_la_LDFLAGS = -version-info 7:0:0 -no-undefined -framework ApplicationServices

extincludedir = $(includedir)/X11/extensions
extinclude_HEADERS = $(top_srcdir)/include/X11/extensions/applewm.h
