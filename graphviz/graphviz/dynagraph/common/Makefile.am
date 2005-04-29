# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
	-I$(top_srcdir)/dynagraph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/agraph

LIBS =

noinst_HEADERS = BezCoeffs.h ColorByAge.h Dynagraph.h GeomUtil.h Geometry.h \
	LGraph-cdt.h LGraph.h PathPlan.h LineTricks.h \
	Solvers.h StrAttr.h StreamGeom.h StringDict.h Transform.h \
	ag2str.h stringsIn.h breakList.h bufferGraphStream.h colors.h \
	derivable_dt.h dgxep.h diff_strgraph.h dt.h ellipse2bezier.h \
	emitGraph.h freelist.h genpoly.h moremath.h parsestr.h reorient.h \
	stringsOut.h time-o-matic.h traversal.h useful.h weightedMedian.h

if ENABLE_DYNAGRAPH
noinst_LTLIBRARIES = libdynagraph.la
endif

libdynagraph_la_SOURCES = Dynagraph.cpp ColorByAge.cpp LabelPlacer.cpp \
	Line.cpp PolyBarriers.cpp Region.cpp Route.cpp ShapeGenerator.cpp \
	Shortest.cpp Solvers.cpp StringDict.cpp Transform.cpp ag2str.cpp \
	stringsIn.cpp ellipse2bezier.cpp emitGraph.cpp genpoly.cpp \
	parsestr.cpp stringsOut.cpp time-o-matic.cpp useful.cpp xlines.c \
	colors.cpp ${noinst_HEADERS} 

EXTRA_DIST = $(libdynagraph_la_SOURCES)
