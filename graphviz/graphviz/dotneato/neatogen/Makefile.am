## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
        -I$(top_srcdir)/dotneato/pack \
        -I$(top_srcdir)/dotneato/gvrender \
        -I$(top_srcdir)/graph \
        -I$(top_srcdir)/cdt \
        -I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = adjust.h edges.h geometry.h heap.h hedges.h info.h mem.h \
	neato.h poly.h neatoprocs.h simple.h site.h voronoi.h
# pkglib_LTLIBRARIES = libneato.la
noinst_LTLIBRARIES = libneatoconvenience.la

libneato_la_SOURCES = adjust.c circuit.c edges.c find_ints.c geometry.c heap.c \
	hedges.c info.c neatoinit.c intersect.c legal.c lu.c matinv.c memory.c \
	poly.c printvis.c site.c solve.c neatosplines.c stuff.c voronoi.c \
	${pkginclude_HEADERS}

libneatoconvenience_la_SOURCES = $(libneato_la_SOURCES)

EXTRA_DIST = Makefile.old
