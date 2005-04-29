## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/graph \
	-I$(top_srcdir)/agraph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan \
	-I$(top_srcdir)/dotneato/pack \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/dotneato/common \
	@EXPAT_INCLUDES@

LIBS =  @LIBS@

man_MANS = acyclic.1 gvcolor.1 gxl2dot.1 dot2gxl.1 nop.1 tred.1 ccomps.1 gc.1 sccmap.1 unflatten.1 gvpack.1 dijkstra.1 bcomps.1

bin_PROGRAMS = gc gvcolor gxl2dot dot2gxl acyclic nop ccomps sccmap tred unflatten gvpack dijkstra bcomps

noinst_HEADERS = ingraphs.h colortbl.h convert.h

noinst_LTLIBRARIES = libingraphs.la

dot2gxl.1: $(top_srcdir)/tools/src/gxl2dot.1
	cp -f $(top_srcdir)/tools/src/gxl2dot.1 $@

libingraphs_la_SOURCES = ingraphs.c ingraphs.h

gxl2dot_SOURCES = cvtgxl.c dot2gxl.c gxl2dot.c convert.h

gxl2dot_LDADD = \
	libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la \
	$(top_builddir)/graph/libgraph.la \
	${EXPAT_LIBS}

dot2gxl_SOURCES = $(gxl2dot_SOURCES)

dot2gxl_LDADD = $(gxl2dot_LDADD)

sccmap_SOURCES = sccmap.c

sccmap_LDADD = \
	libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

ccomps_SOURCES = ccomps.c

ccomps_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

tred_SOURCES = tred.c

tred_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

unflatten_SOURCES = unflatten.c

unflatten_LDADD = \
	libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

acyclic_SOURCES = acyclic.c

acyclic_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

nop_SOURCES = nop.c

nop_LDADD = \
	libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

gvcolor_SOURCES = gvcolor.c colxlate.c colortbl.h

gvcolor_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

bcomps_SOURCES = bcomps.c

bcomps_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

gc_SOURCES = gc.c

gc_LDADD = \
	libingraphs.la \
	$(top_builddir)/graph/libgraph.la \
	$(top_builddir)/cdt/libcdt.la

gvpack_SOURCES = gvpack.c

gvpack_LDADD = \
	libingraphs.la \
	$(top_builddir)/dotneato/neatogen/libneatoconvenience.la \
	$(top_builddir)/dotneato/pack/libpackconvenience.la \
	$(top_builddir)/dotneato/common/libcommonconvenience.la \
	$(top_builddir)/dotneato/gvrender/libgvrenderconvenience.la \
	$(top_builddir)/pathplan/libpathplanconvenience.la \
	$(top_builddir)/gd/libgdconvenience.la \
	$(top_builddir)/graph/libgraphconvenience.la \
	$(top_builddir)/cdt/libcdtconvenience.la

dijkstra_SOURCES = dijkstra.c

dijkstra_LDADD = \
	libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

EXTRA_DIST = $(man_MANS) Makefile.old

CLEANFILES = stamp.h dot2gxl.1
