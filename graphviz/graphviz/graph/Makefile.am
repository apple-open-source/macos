## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = -I$(top_srcdir) \
	-I$(top_srcdir)/cdt

LIBS =

pkginclude_HEADERS = graph.h libgraph.h triefa.h parser.h agxbuf.h
pkglib_LTLIBRARIES = libgraph.la
noinst_LTLIBRARIES = libgraphconvenience.la
man_MANS = graph.3

libgraph_la_SOURCES = attribs.c agxbuf.c edge.c graph.c graphio.c lexer.c \
	node.c parser.y refstr.c trie.c $(pkginclude_HEADERS)

libgraphconvenience_la_SOURCES = $(libgraph_la_SOURCES)

$(libgraphconvenience_la_OBJECTS): parser.c parser.h

parser.c: y.output
	@SED@ "s/yy/ag/g" < y.tab.c > parser.c

parser.h: y.output
	@SED@ "s/yy/ag/g" < y.tab.h > parser.h

y.output: ${top_srcdir}/graph/parser.y
	@YACC@ -dv ${top_srcdir}/graph/parser.y

# this is a home-made tool
# trie.c : trie_input
#       triegen  < trie_input > trie.c

DISTCLEANFILES = y.output parser.c parser.h y.tab.[ch]

EXTRA_DIST = $(man_MANS) triefa.cP parser.grammar \
	Makefile.old parser.c parser.h
