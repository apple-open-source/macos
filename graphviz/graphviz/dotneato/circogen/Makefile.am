## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
        -I$(top_srcdir)/dotneato/pack \
        -I$(top_srcdir)/dotneato/dotogen \
        -I$(top_srcdir)/dotneato/neatogen \
        -I$(top_srcdir)/dotneato/gvrender \
        -I$(top_srcdir)/graph \
        -I$(top_srcdir)/cdt \
        -I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = graphviz_block.h blockpath.h blocktree.h circo.h \
	circpos.h circular.h deglist.h edgelist.h nodelist.h \
	nodeset.h stack.h
# pkglib_LTLIBRARIES = libcirco.la
noinst_LTLIBRARIES = libcircoconvenience.la

libcirco_la_SOURCES = circularinit.c nodelist.c block.c edgelist.c \
	stack.c circular.c deglist.c blocktree.c blockpath.c \
	circpos.c nodeset.c \
	${pkginclude_HEADERS}

libcircoconvenience_la_SOURCES = $(libcirco_la_SOURCES)

EXTRA_DIST = Makefile.old
