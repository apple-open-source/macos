## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)

pkginclude_HEADERS = cdt.h dthdr.h
noinst_LTLIBRARIES = libcdtconvenience.la
pkglib_LTLIBRARIES = libcdt.la
man_MANS = cdt.3

pkgextraincludedir = ${pkgincludedir}
pkgextrainclude_DATA = ${top_srcdir}/ast_common.h

libcdt_la_SOURCES = dtclose.c dtdisc.c dtextract.c dtflatten.c \
	dthash.c dtlist.c dtmethod.c dtopen.c dtrenew.c dtrestore.c dtsize.c \
	dtstat.c dtstrhash.c dttree.c dtview.c dtwalk.c \
	$(pkginclude_HEADERS)

libcdtconvenience_la_SOURCES = ${libcdt_la_SOURCES}

EXTRA_DIST = $(man_MANS) Makefile.old features
