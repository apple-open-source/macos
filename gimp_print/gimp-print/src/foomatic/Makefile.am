## $Id: Makefile.am,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $
## Copyright (C) 2000 Roger Leigh
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 gnu

@SET_MAKE@

MAINT_CHARSET = latin1


## Variables

AM_CFLAGS = $(GNUCFLAGS)
INCLUDES = @INCLUDES@ $(LIBGIMPPRINT_CFLAGS) -I$(top_srcdir)/src/main
LIBPRINTUT = $(top_builddir)/lib/libprintut.la
LIBS = $(INTLLIBS) @LIBS@


## Programs

FOOMATIC_NOINST_BIN = printer_options printer_margins paper_sizes gimp-print-version \
	stp_limits printers

noinst_PROGRAMS = @FOOMATIC_NOINST_BIN@

EXTRA_PROGRAMS = printer_options printer_margins gimp-print-version paper_sizes \
	stp_limits printers

printer_options_SOURCES = printer_options.c
printer_options_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
printer_options_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

printer_margins_SOURCES = printer_margins.c
printer_margins_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
printer_margins_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

gimp_print_version_SOURCES = gimp-print-version.c
gimp_print_version_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
gimp_print_version_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

paper_sizes_SOURCES = paper_sizes.c
paper_sizes_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
paper_sizes_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

stp_limits_SOURCES = stp_limits.c
stp_limits_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
stp_limits_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

printers_SOURCES = printers.c
printers_LDADD = $(LIBPRINTUT) $(LIBGIMPPRINT_LIBS)
printers_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)


## Rules

if BUILD_FOOMATIC
INSTALL_DATA_LOCAL_DEPS = foomatic-data
else
INSTALL_DATA_LOCAL_DEPS =
endif

if BUILD_FOOMATIC_3
FOOMATIC_GENERATOR_FOOMATIC_3_OPTS = -f 3
else
FOOMATIC_GENERATOR_FOOMATIC_3_OPTS =
endif

if BUILD_IJS
FOOMATIC_GENERATOR_EXTRA_OPTS = -d all
FOOMATIC_IJS = yes
else
FOOMATIC_GENERATOR_EXTRA_OPTS = -d noijs
FOOMATIC_IJS =
endif

all-local: $(INSTALL_DATA_LOCAL_DEPS)

install-data-local: $(INSTALL_DATA_LOCAL_DEPS)
	if test -n "$(INSTALL_DATA_LOCAL_DEPS)" ; then \
	  $(MAKE) install-kit FOOMATIC_DB=foomatic-db/gimp-print ; \
	  if test -n "$(FOOMATIC_IJS)" ; then \
	    $(MAKE) install-kit FOOMATIC_DB=foomatic-db/gimp-print-ijs ; \
	  fi ; \
	fi

install-kit:
	if test -n "$(DESTDIR)" ; then \
	  $(mkinstalldirs) $(DESTDIR)`$(FOOMATIC_KITLOAD) -l` ; \
	  $(FOOMATIC_KITLOAD) -f -d $(DESTDIR) -k $(FOOMATIC_DB) ; \
	else \
	  $(FOOMATIC_KITLOAD) -f -k $(FOOMATIC_DB) ; \
	fi

clean-local:
	-$(RM) -r foomatic-db

foomatic-data: foomatic-stamp

foomatic-stamp: foomatic-generator foomatic-printermap $(EXTRA_PROGRAMS)
	-$(RM) -r foomatic-db
	./foomatic-generator -m $(srcdir)/foomatic-printermap -t $(srcdir)/foomatic-templates $(FOOMATIC_GENERATOR_EXTRA_OPTS) $(FOOMATIC_GENERATOR_FOOMATIC_3_OPTS)
	touch foomatic-stamp

$(LIBGIMPPRINT_LIBS):
	cd ../main ; \
	$(MAKE)


## Clean

CLEANFILES = $(noinst_PROGRAMS) foomatic-stamp
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = foomatic-printermap 		\
	foomatic-templates-gs/*.xml		\
	foomatic-templates-ijs/*.xml

