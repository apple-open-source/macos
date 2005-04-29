## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = common dynadag fdp graphsearch incrface shortspline voronoi

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/dynagraph \
	-I$(top_srcdir)/agraph \
	-I$(top_srcdir)/cdt

LIBS = @LIBS@

CFLAGS = @CFLAGS@ -DDATE="\"`date`\""

noinst_HEADERS = 
man_MANS = 

if ENABLE_DYNAGRAPH
bin_PROGRAMS = dynagraph
noinst_PROGRAMS = gengraph
endif

gengraph_SOURCES = gengraph.cpp

gengraph_LDADD = $(top_builddir)/dynagraph/common/libdynagraph.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

dynagraph_SOURCES = main.cpp

dynagraph_LDADD = $(top_builddir)/dynagraph/incrface/libincrface.la \
	$(top_builddir)/dynagraph/fdp/libfdp.la \
	$(top_builddir)/dynagraph/voronoi/libvoronoi.la \
	$(top_builddir)/dynagraph/shortspline/libshortspline.la \
	$(top_builddir)/dynagraph/dynadag/libdynadag.la \
	$(top_builddir)/dynagraph/common/libdynagraph.la \
	$(top_builddir)/pathplan/libpathplan.la \
        $(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

EXTRA_DIST = \
        $(gengraph_SOURCES) \
        $(dynagraph_SOURCES) \
        $(man_MANS) \
        dynagraph.html \
        plainmake.zip
