## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = no-dependencies foreign

EXTRA_DIST = acinclude.m4

noinst_LTLIBRARIES = libfoo.la

libfoo_la_SOURCES = foo.c
libfoo_la_LIBADD = $(LIBM)

noinst_HEADERS = foo.h

bin_PROGRAMS = cdemo

cdemo_SOURCES = main.c
cdemo_LDADD = libfoo.la

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
