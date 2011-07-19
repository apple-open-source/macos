lib_LTLIBRARIES = libxkbui.la

libxkbui_la_SOURCES = \
         XKBui.c \
         XKBuiPriv.h

INCLUDES = -I$(top_srcdir)/include/X11/extensions

AM_CFLAGS = @XKBUI_CFLAGS@

libxkbui_la_LIBADD = @XKBUI_LIBS@

libxkbui_la_LDFLAGS = -version-number 1:0:0 -no-undefined

libxkbuiincludedir = $(includedir)/X11/extensions
libxkbuiinclude_HEADERS = \
    $(top_srcdir)/include/X11/extensions/XKBui.h
