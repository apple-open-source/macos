## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/tools/expr \
	-I$(top_builddir)/tools/expr \
	-I$(top_srcdir)/tools/vmalloc \
	-I$(top_srcdir)/tools/sfio \
	-I$(top_srcdir)/tools/ast \
	-I$(top_srcdir)/tools/src \
	-I$(top_srcdir)/agraph \
	-I$(top_srcdir)/cdt

LIBS = @LIBS@

bin_PROGRAMS = gvpr

noinst_PROGRAMS = mkdefs

gvpr_SOURCES = actions.c compile.c gvpr.c gprstate.c \
	parse.c queue.c \
	actions.h compile.h gprstate.h parse.h queue.h

gvpr_LDADD = $(top_builddir)/tools/expr/libexpr.la \
	$(top_builddir)/tools/ast/libast.la \
        $(top_builddir)/tools/vmalloc/libvmalloc.la \
	$(top_builddir)/tools/sfio/libsfio.la \
        $(top_builddir)/tools/src/libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
        $(top_builddir)/cdt/libcdt.la

$(gvpr_OBJECTS): gdefs.h

mkdefs_SOURCES = mkdefs.c

gdefs.h: ${top_srcdir}/tools/gpr/gprdata mkdefs
	./mkdefs gdefs.h < ${top_srcdir}/tools/gpr/gprdata

man_MANS = gvpr.1

EXTRA_DIST = $(man_MANS) Makefile.old gprdata mkdefs.c

CLEANFILES = gdefs.h mkdefs
