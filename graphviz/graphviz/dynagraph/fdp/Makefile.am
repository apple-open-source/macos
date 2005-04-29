## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/dynagraph \
	-I$(top_srcdir)/agraph

LIBS =

noinst_HEADERS = FVSCombo.h fdp.h grid.h macros.h

if ENABLE_DYNAGRAPH
noinst_LTLIBRARIES = libfdp.la
endif

libfdp_la_SOURCES = grid.cpp layout.cpp process.cpp

EXTRA_DIST = $(libfdp_la_SOURCES)
