if XFONT_FONTCACHE
INCLUDES =                                \
	-I${top_srcdir}/include
noinst_LTLIBRARIES = libfontcache.la
AM_CFLAGS = $(FONTCACHEPROTO_CFLAGS)
libfontcache_la_SOURCES = fontcache.c fontcache.h fcqueue.h
fontdir = $(includedir)/X11/fonts
endif
