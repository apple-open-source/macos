## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/pack \
	-I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = circle.h
# pkglib_LTLIBRARIES = libtwopi.la
noinst_LTLIBRARIES = libtwopiconvenience.la

libtwopi_la_SOURCES = twopiinit.c circle.c ${pkginclude_HEADERS}

libtwopiconvenience_la_SOURCES = $(libtwopi_la_SOURCES)

EXTRA_DIST = Makefile.old
