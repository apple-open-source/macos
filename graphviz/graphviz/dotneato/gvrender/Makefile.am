## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
	-I$(top_srcdir)/dotneato/common \
	-I$(top_srcdir)/pathplan \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt

LIBS = 

pkginclude_HEADERS = gvrender.h gvrenderint.h gvre.h
# pkglib_LTLIBRARIES = libgvrender.la
noinst_LTLIBRARIES = libgvrenderconvenience.la

libgvrender_la_SOURCES = gvrender.c ${pkginclude_HEADERS} 

libgvrenderconvenience_la_SOURCES = $(libgvrender_la_SOURCES)

EXTRA_DIST = Makefile.old
