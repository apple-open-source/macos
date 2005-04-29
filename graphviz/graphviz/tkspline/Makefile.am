## Process this file with automake to produce Makefile.in

INCLUDES = -I@TK_SRC_DIR@/generic -I@TCL_SRC_DIR@/generic \
	@TCL_INCLUDES@ @TCLINT_INCLUDES@ \
	@TK_INCLUDES@ @TKINT_INCLUDES@

LIBS =

AM_CFLAGS = @X_CFLAGS@ @TCL_CFLAGS@  @TK_CFLAGS@ @TK_DEFS@

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = demo

if WITH_TK
pkglib_LTLIBRARIES = libtkspline.la
man_MANS = tkspline.n
endif

libtkspline_la_SOURCES = tkspline.c

libtkspline_la_LIBADD = \
	$(top_builddir)/tkstubs/libtkstubs.la \
	$(top_builddir)/tclstubs/libtclstubs.la 

all-local: pkgIndex.tcl

pkgIndex.tcl:
	echo 'package ifneeded Tkspline $(VERSION) "' > pkgIndex.tcl
	echo '	package require Tk 8.3' >> pkgIndex.tcl
	echo '	load [file join $$dir libtkspline$(TCL_SHLIB_SUFFIX)] Tkspline"' >> pkgIndex.tcl

CLEANFILES = pkgIndex.tcl

EXTRA_DIST = $(libtkspline_la_SOURCES) tkspline.n dllEntry.c
