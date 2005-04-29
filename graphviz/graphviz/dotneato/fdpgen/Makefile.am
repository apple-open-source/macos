## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
        -I$(top_srcdir)/dotneato/pack \
        -I$(top_srcdir)/dotneato/neatogen \
        -I$(top_srcdir)/dotneato/gvrender \
        -I$(top_srcdir)/graph \
        -I$(top_srcdir)/cdt \
        -I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = comp.h dbg.h fdp.h grid.h options.h tlayout.h xlayout.h
# pkglib_LTLIBRARIES = libfdp.la
noinst_LTLIBRARIES = libfdpconvenience.la

libfdp_la_SOURCES = comp.c dbg.c grid.c init.c layout.c options.c \
	tlayout.c xlayout.c \
	${pkginclude_HEADERS}

libfdpconvenience_la_SOURCES = $(libfdp_la_SOURCES)

EXTRA_DIST = Makefile.old
