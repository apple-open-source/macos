## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/tools/src \
	-I$(top_srcdir)/agraph \
	-I$(top_srcdir)/cdt

bin_PROGRAMS = prune

prune_SOURCES = generic_list.c prune.c \
	generic_list.h

noinst_HEADERS = generic_list.h

prune_LDADD = $(top_builddir)/tools/src/libingraphs.la \
	$(top_builddir)/agraph/libagraph.la \
	$(top_builddir)/cdt/libcdt.la

man_MANS = prune.1

EXTRA_DIST = $(man_MANS) Makefile.old Makefile.orig README test.dot
