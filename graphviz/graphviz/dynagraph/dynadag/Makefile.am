## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/dynagraph

LIBS =

noinst_HEADERS = DynaDAG.h Measurements.h Medians.h SiftMatrix.h ns.h nsdat.h

if ENABLE_DYNAGRAPH
noinst_LTLIBRARIES = libdynadag.la
endif

libdynadag_la_SOURCES = Check.cpp Config.cpp ConstraintGraph.cpp \
	DotlikeOptimizer.cpp DynaDAG.cpp FlexiSpline.cpp \
	getCrossoptModelNodes.cpp Measurements.cpp Medians.cpp \
	Ranker.cpp SetYs.cpp SiftMatrix.cpp Spline.cpp \
	Update.cpp UVCross.cpp XSolver.cpp ${noinst_HEADERS}

EXTRA_DIST = \
        $(libdynadag_la_SOURCES) \
        HybridOptimizer.cpp \
        HybridOptimizer2.cpp \
	MedianShuffle.cpp \
        MedianTwiddle.cpp \
        ConstraintGraph.cpp \
	Sifter.cpp \
        PathOptim.cpp
