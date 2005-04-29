## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/lefty

LIBS =

if WITH_X
noinst_LTLIBRARIES = libos.la
endif

libos_la_SOURCES = io.c

EXTRA_DIST = $(libos_la_SOURCES)
