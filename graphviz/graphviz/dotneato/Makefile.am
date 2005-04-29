## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = gvrender common pack dotgen neatogen twopigen fdpgen circogen gvre_svg

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/gvrender \
        -I$(top_srcdir)/dotneato/common \
	-I$(top_srcdir)/dotneato/dotgen \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/twopigen \
	-I$(top_srcdir)/dotneato/fdpgen \
	-I$(top_srcdir)/dotneato/circogen \
	-I$(top_srcdir)/dotneato/pack \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan

bin_PROGRAMS = dot neato twopi fdp circo
bin_SCRIPTS = dotneato-config
pkginclude_HEADERS = dotneato.h
noinst_PROGRAMS = dotmemtest neatomemtest twopimemtest fdpmemtest
pkglib_LTLIBRARIES = libdotneato.la
man_MANS = dot.1 neato.1 twopi.1 fdp.1 circo.1 dotneato-config.1

libdotneato_la_SOURCES = dotneato.c dotneato.h
dot_SOURCES = dot.c
neato_SOURCES = neato.c
twopi_SOURCES = twopi.c
fdp_SOURCES = fdp.c
circo_SOURCES = circo.c
dotmemtest_SOURCES = dotmemtest.c
neatomemtest_SOURCES = neatomemtest.c
twopimemtest_SOURCES = twopimemtest.c
fdpmemtest_SOURCES = fdpmemtest.c

libdotneato_la_LIBADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/dotneato/dotgen/libdotconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/twopigen/libtwopiconvenience.la \
	$(top_builddir)/dotneato/fdpgen/libfdpconvenience.la \
	$(top_builddir)/dotneato/circogen/libcircoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/pathplan/libpathplanconvenience.la \
	$(top_builddir)/graph/libgraphconvenience.la \
	$(top_builddir)/cdt/libcdtconvenience.la \
	$(top_builddir)/gd/libgdconvenience.la

dot_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/dotgen/libdotconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

neato_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

twopi_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/twopigen/libtwopiconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

fdp_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/fdpgen/libfdpconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

circo_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/circogen/libcircoconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

dotmemtest_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/dotgen/libdotconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

neatomemtest_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

twopimemtest_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/twopigen/libtwopiconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
        $(top_builddir)/graph/libgraph.la \
        $(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

fdpmemtest_LDADD = \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/fdpgen/libfdpconvenience.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplan.la \
        $(top_builddir)/graph/libgraph.la \
        $(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/gd/libgdconvenience.la

EXTRA_DIST = $(man_MANS) XFIG_FORMAT3.2.txt Makefile.old \
	dotneato-config.in awk
