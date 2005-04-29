## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES =
LIBS =

pkginclude_HEADERS = pathgeom.h pathplan.h pathutil.h solvers.h \
	tri.h vis.h vispath.h
pkglib_LTLIBRARIES = libpathplan.la
noinst_LTLIBRARIES = libpathplanconvenience.la
man_MANS = pathplan.3

libpathplan_la_SOURCES = cvt.c inpoly.c route.c shortest.c \
	shortestpth.c solvers.c triang.c util.c visibility.c \
	${pkginclude_HEADERS}

libpathplanconvenience_la_SOURCES = ${libpathplan_la_SOURCES}

EXTRA_DIST = $(man_MANS) Makefile.old path.lefty
