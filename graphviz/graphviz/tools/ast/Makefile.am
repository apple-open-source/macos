## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/tools/sfio

LIBS =

noinst_HEADERS = align.h ast.h error.h hashkey.h

noinst_LTLIBRARIES = libast.la
man_MANS = 

libast_la_SOURCES = pathpath.c sfstr.h chresc.c chrtoi.c error.c \
	fmtbuf.c fmtesc.c pathaccess.c pathbin.c pathcanon.c pathcat.c \
	pathexists.c pathfind.c pathgetlink.c \
	strcopy.c stresc.c strmatch.c strton.c strerror.c \
	align.h ast.h error.h hashkey.h

CLEANFILES = 

EXTRA_DIST = $(man_MANS) Makefile.old
