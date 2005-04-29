## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = pack.h
pkglib_LTLIBRARIES = libpack.la
noinst_LTLIBRARIES = libpackconvenience.la
man_MANS = pack.3

libpack_la_SOURCES = ccomps.c pack.c ${pkginclude_HEADERS}

libpackconvenience_la_SOURCES = $(libpack_la_SOURCES)

EXTRA_DIST = $(man_MANS) Makefile.old ptest.c
