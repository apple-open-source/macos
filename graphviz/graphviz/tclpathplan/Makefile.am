## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/tclhandle \
	-I$(top_srcdir)/pathplan \
	@TCL_INCLUDES@

LIBS = @LIBS@

AM_CFLAGS = @TCL_CFLAGS@

noinst_HEADERS = simple.h
if WITH_TCL
pkglib_LTLIBRARIES = libtclplan.la
endif

SUBDIRS = demo

libtclplan_la_SOURCES = find_ints.c intersect.c makecw.c \
	tclpathplan.c wrapper.c

libtclplan_la_LIBADD = \
	$(top_builddir)/tclhandle/libtclhandle.la \
	$(top_builddir)/pathplan/libpathplanconvenience.la \
	$(top_builddir)/tclstubs/libtclstubs.la

all-local: pkgIndex.tcl

pkgIndex.tcl:
	echo 'package ifneeded Tclpathplan $(VERSION) "' > pkgIndex.tcl
	echo '	load [file join $$dir libtclplan$(TCL_SHLIB_SUFFIX)] Tclpathplan"' >> pkgIndex.tcl

CLEANFILES = pkgIndex.tcl

EXTRA_DIST = $(libtclplan_la_SOURCES)
