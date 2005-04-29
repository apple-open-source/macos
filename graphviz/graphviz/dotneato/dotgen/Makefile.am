## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/common \
        -I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan

LIBS =

pkginclude_HEADERS = dot.h dotprocs.h
# pkglib_LTLIBRARIES = libdot.la
noinst_LTLIBRARIES = libdotconvenience.la

libdot_la_SOURCES = acyclic.c class1.c class2.c cluster.c compound.c \
	conc.c decomp.c fastgr.c flat.c dotinit.c mincross.c \
	ns.c position.c rank.c routespl.c sameport.c dotsplines.c \
	${pkginclude_HEADERS}

libdotconvenience_la_SOURCES = $(libdot_la_SOURCES)

EXTRA_DIST = Makefile.old
