## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/dotgen \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/twopigen \
	-I$(top_srcdir)/dotneato/fdpgen \
	-I$(top_srcdir)/dotneato/circogen \
	-I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/dotneato/common \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan \
	-I$(top_srcdir)/tclhandle \
	@TCL_INCLUDES@

LIBS =  @LIBS@ -lc

AM_CFLAGS = @TCL_CFLAGS@

SUBDIRS = demo

noinst_HEADERS = tcldot.h
if WITH_TCL
pkglib_LTLIBRARIES = libtcldot.la
man_MANS = tcldot.n
endif

libtcldot_la_SOURCES = tcldot.c tkgen.c tcldot.h

libtcldot_la_LIBADD = \
	$(top_builddir)/tclhandle/libtclhandle.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
        $(top_builddir)/dotneato/dotgen/libdotconvenience.la \
        $(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
        $(top_builddir)/dotneato/twopigen/libtwopiconvenience.la \
        $(top_builddir)/dotneato/fdpgen/libfdpconvenience.la \
        $(top_builddir)/dotneato/circogen/libcircoconvenience.la \
        $(top_builddir)/dotneato/pack/libpackconvenience.la \
        $(top_builddir)/pathplan/libpathplanconvenience.la \
        $(top_builddir)/graph/libgraphconvenience.la \
	$(top_builddir)/cdt/libcdtconvenience.la \
	$(top_builddir)/gd/libgdconvenience.la \
	$(top_builddir)/gdtclft/gdtclft.lo \
	$(top_builddir)/tclstubs/libtclstubs.la

all-local: pkgIndex.tcl

pkgIndex.tcl:
	echo 'package ifneeded Tcldot $(VERSION) "' > pkgIndex.tcl
	echo '	load [file join $$dir libtcldot$(TCL_SHLIB_SUFFIX)] Tcldot"' >> pkgIndex.tcl

CLEANFILES = pkgIndex.tcl

EXTRA_DIST = $(libtcldot_la_SOURCES) tcldot.n README README.Tkspline \
	Makefile.old
