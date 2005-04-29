## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/tclhandle \
	-I$(top_srcdir)/gd \
	@TCL_INCLUDES@

LIBS = @LIBS@ -lc

AM_CFLAGS = @TCL_CFLAGS@

SUBDIRS = demo

if WITH_TCL
pkglib_LTLIBRARIES = libgdtclft.la
man_MANS = gdtclft.n
endif

libgdtclft_la_SOURCES = gdtclft.c

libgdtclft_la_LIBADD = \
	$(top_builddir)/tclhandle/libtclhandle.la \
	$(top_builddir)/gd/libgdconvenience.la \
	$(top_builddir)/tclstubs/libtclstubs.la

all-local: pkgIndex.tcl

pkgIndex.tcl:
	echo 'package ifneeded Gdtclft $(VERSION) "' > pkgIndex.tcl
	echo '	load [file join $$dir libgdtclft$(TCL_SHLIB_SUFFIX)] Gdtclft"' >> pkgIndex.tcl

test:
	echo 'load .libs/libgdtclft$(TCL_SHLIB_SUFFIX) Gdtclft ; cd tests ; source all' | tclsh8.3

CLEANFILES = pkgIndex.tcl

EXTRA_DIST = $(libgdtclft_la_SOURCES) gdtclft.n Makefile.old
