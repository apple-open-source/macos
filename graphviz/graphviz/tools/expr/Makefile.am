## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/tools/vmalloc \
	-I$(top_srcdir)/tools/sfio \
	-I$(top_srcdir)/tools/ast \
	-I$(top_srcdir)/cdt

LIBS =

pkginclude_HEADERS = exgram.h exlib.h expr.h
pkglib_LTLIBRARIES = libexpr.la
noinst_LTLIBRARIES = libexprconvenience.la
man_MANS = expr.3

libexpr_la_SOURCES = excc.c excontext.c exdata.c exerror.c \
	exeval.c exexpr.c exlexname.c exopen.c exrewind.c extoken.c \
	extype.c exzero.c exparse.y ${pkginclude_HEADERS}

libexprconvenience_la_SOURCES = ${libexpr_la_SOURCES}

$(libexprconvenience_la_OBJECTS): exparse.h exparse.c exgram.h exop.h

y.output: ${top_srcdir}/tools/expr/exparse.y
	@YACC@ -dtv ${top_srcdir}/tools/expr/exparse.y

exparse.c: y.output 
	@SED@ "s/yy/ex/g" <y.tab.c | \
		@SED@ "s/YY/EX/g" | \
		@SED@ "s/fprintf/sfprintf/g" | \
		@SED@ "s/FILE/Sfio_t/g" | \
		@SED@ "s/stderr/sfstderr/g" > exparse.c

exparse.h: y.output
	echo "#ifndef _EXPARSE_H" > exparse.h
	echo "#define _EXPARSE_H" >> exparse.h
	@SED@ "s/yy/ex/g" < y.tab.h | @SED@ "s/YY/EX/g" >> exparse.h
	echo "#endif /* _EXPARSE_H */" >> exparse.h

exop.h: exparse.h
	echo "static const char* exop[] = {" > exop.h
	@SED@ -e '/^[ 	]*#[ 	]*define[ 	][ 	]*[A-Z]/!d' -e 's/^[ 	]*#[ 	]*define[ 	]*\([A-Z0-9_]*\).*/	"\1",/' < exparse.h >> exop.h
	echo "};" >> exop.h

CLEANFILES = y.tab.c y.tab.h y.output

DISTCLEANFILES = exparse.c exparse.h exop.h

EXTRA_DIST = $(man_MANS) RELEASE Makefile.nmake Makefile.orig Makefile.old \
	exparse.c exparse.h
