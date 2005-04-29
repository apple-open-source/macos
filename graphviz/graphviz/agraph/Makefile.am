## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/cdt

pkginclude_HEADERS = aghdr.h agraph.h malloc.h vmstub.h grammar.h
pkglib_LTLIBRARIES = libagraph.la
noinst_LTLIBRARIES = libagraphconvenience.la
man_MANS = agraph.3

libagraph_la_SOURCES = agerror.c apply.c attr.c \
	edge.c flatten.c grammar.y graph.c id.c imap.c io.c \
	mem.c node.c obj.c pend.c rec.c refstr.c scan.l subg.c \
	utils.c write.c

libagraphconvenience_la_SOURCES = ${libagraph_la_SOURCES}

scan.o scan.lo: scan.c grammar.h

scan.c: ${top_srcdir}/agraph/scan.l
	@LEX@ -i ${top_srcdir}/agraph/scan.l
	@SED@ "s/yy/aag/g" < @LEX_OUTPUT_ROOT@.c > scan.c
	rm @LEX_OUTPUT_ROOT@.c

y.output: ${top_srcdir}/agraph/grammar.y
	@YACC@ -dv ${top_srcdir}/agraph/grammar.y

grammar.c: y.output
	@SED@ "s/yy/aag/g" < y.tab.c > grammar.c

grammar.h: y.output
	@SED@ "s/yy/aag/g" < y.tab.h > grammar.h

CLEANFILES = grammar.[ch] scan.[ch] y.output y.tab.[ch] 

EXTRA_DIST = $(man_MANS) Makefile.old cmpnd.c dotdge.c \
	main.c tester.c README

