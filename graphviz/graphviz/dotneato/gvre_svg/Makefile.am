## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
	-I$(top_srcdir)/dotneato/common \
        -I$(top_srcdir)/dotneato/gvrender \
        -I$(top_srcdir)/pathplan \
        -I$(top_srcdir)/graph \
        -I$(top_srcdir)/cdt \
        ${EXPAT_INCLUDES}

LIBS = 

pkginclude_HEADERS = 

if ENABLE_GVRENDER
pkglib_LTLIBRARIES = libgvre_svg.la
noinst_LTLIBRARIES = libgvre_svgconvenience.la
endif

libgvre_svg_la_SOURCES = gvre_svg.c ${pkginclude_HEADERS} 

libgvre_svgconvenience_la_SOURCES = $(libgvre_svg_la_SOURCES)
