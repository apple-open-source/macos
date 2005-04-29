## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/agutil \
	-I$(top_srcdir)/agraph \
	-I$(top_srcdir)/cdt

LIBS =

noinst_HEADERS = ns.h nspvt.h
noinst_LTLIBRARIES = libns.la

libns_la_SOURCES = ns.c ns.h

EXTRA_DIST =
