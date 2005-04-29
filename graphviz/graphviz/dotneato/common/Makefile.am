## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
        -I$(top_srcdir) \
        -I$(top_srcdir)/dotneato/dotgen \
	-I$(top_srcdir)/dotneato/neatogen \
	-I$(top_srcdir)/dotneato/twopigen \
	-I$(top_srcdir)/dotneato/gvrender \
	-I$(top_srcdir)/graph \
	-I$(top_srcdir)/cdt \
	-I$(top_srcdir)/pathplan \
	-I$(top_srcdir)/gd \
        ${EXPAT_INCLUDES} \
        ${Z_INCLUDES}

LIBS = ${EXPAT_LIBS}

pkginclude_HEADERS = const.h globals.h htmllex.h htmltable.h macros.h \
	pointset.h render.h renderprocs.h types.h utils.h colortbl.h
# pkglib_LTLIBRARIES = libcommon.la
noinst_LTLIBRARIES = libcommonconvenience.la

libcommon_la_SOURCES = arrows.c colxlate.c emit.c diagen.c figgen.c \
	fontmetrics.c gdgen.c globals.c hpglgen.c htmllex.c htmlparse.y \
	htmltable.c input.c mapgen.c mifgen.c mpgen.c output.c pointset.c \
	postproc.c picgen.c psgen.c shapes.c splines.c svggen.c timing.c \
	strcasecmp.c strncasecmp.c utils.c vrmlgen.c vtxgen.c \
	${pkginclude_HEADERS} ps.txt color_names

libcommonconvenience_la_SOURCES = $(libcommon_la_SOURCES)

psgen.o psgen.lo : ps.h

ps.h : ps.txt
	$(AWK) -f $(top_srcdir)/dotneato/awk/stringize.awk ps.txt > ps.h

colxlate.o colxlate.lo : colortbl.h

colortbl.h : color_lib
	$(AWK) -f $(top_srcdir)/dotneato/awk/colortbl.awk color_lib > colortbl.h

color_lib : $(top_srcdir)/dotneato/common/color_names
	$(SED) s/_//g $(top_srcdir)/dotneato/common/color_names | LC_COLLATE=C $(SORT) > color_lib

htmllex.o htmllex.lo: htmllex.c htmllex.h htmlparse.h
htmlparse.o htmlparse.lo: htmlparse.c htmlparse.h
                                                                                
y.output: ${top_srcdir}/dotneato/common/htmlparse.y
	@YACC@ -dv ${top_srcdir}/dotneato/common/htmlparse.y

htmlparse.c: y.output
	@SED@ "s/yy/html/g" < y.tab.c > htmlparse.c

htmlparse.h: y.output
	@SED@ "s/yy/html/g" < y.tab.h > htmlparse.h

DISTCLEANFILES = color_lib colortbl.h y.output y.tab.[ch] htmlparse.[ch]

EXTRA_DIST = Makefile.old README.imap chars.tcl \
	ps.h htmlparse.c htmlparse.h
