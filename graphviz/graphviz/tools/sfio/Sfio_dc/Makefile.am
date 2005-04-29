## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/tools/sfio

LIBS =

noinst_HEADERS = sfdchdr.h sfdisc.h

noinst_LTLIBRARIES = libsfiodc.la
man_MANS = 

libsfiodc_la_SOURCES = sfdcdio.c sfdcdos.c sfdcfilter.c sfdclzw.c \
	sfdcseekable.c sfdcslow.c sfdcsubstream.c sfdctee.c sfdcunion.c \
	sfdchdr.h sfdisc.h

${top_builddir}/FEATURE/sfio:  ${top_srcdir}/tools/sfio/features/sfio
	mkdir -p ${top_builddir}/FEATURE
	@KSH@ $(top_srcdir)/iffe - set cc $(CC) $(CCMODE) $(CXFLAGS) : run  ${top_srcdir}/tools/sfio/features/sfio > $@

$(libsfiodc_la_OBJECTS): ${top_builddir}/FEATURE/sfio

DISTCLEANFILES = ${top_builddir}/FEATURE/sfio

EXTRA_DIST = $(man_MANS) Makefile.old
