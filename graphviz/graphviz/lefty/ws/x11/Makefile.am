## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

INCLUDES = -I$(top_srcdir)/lefty @XAW_INCLUDES@

LIBS =

AM_CFLAGS =  @X_CFLAGS@

SUBDIRS = libfilereq

noinst_HEADERS = gcommon.h
if WITH_X
noinst_LTLIBRARIES = libws.la
endif

libws_la_SOURCES = garray.c gbutton.c gcanvas.c gcommon.c glabel.c \
	gmenu.c gpcanvas.c gquery.c gscroll.c gtext.c gview.c gcommon.h

EXTRA_DIST = $(libws_la_SOURCES)
