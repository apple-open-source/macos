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

PACKAGE = cups

bindir = @cups_bindir@

pkgdatadir = $(cups_conf_datadir)
pkglibdir = $(cups_conf_serverbin)
pkgsysconfdir = $(cups_conf_serverroot)

cupsexec_backenddir = $(pkglibdir)/backend
cupsexec_filterdir = $(pkglibdir)/filter
cups_modeldir = $(pkgdatadir)/model

AM_CFLAGS = $(GNUCFLAGS)
DEFS = @CUPS_CFLAGS@ @DEFS@ -I. -I$(srcdir) -I../.. -DCUPS_DATADIR=\"$(pkgdatadir)\" -DGIMP_PRINT_VERSION=\"$(VERSION)\"
INCLUDES = @INCLUDES@ $(LIBGIMPPRINT_CFLAGS)
LIBPRINTUT = $(top_builddir)/lib/libprintut.la
LIBS = $(INTLLIBS) @LIBS@


## Programs

CUPS_BIN = cups-calibrate
CUPS_BACKEND_BIN = epson canon
CUPS_FILTER_BIN = rastertoprinter commandtoepson commandtocanon
CUPS_NOINST_BIN = genppd

bin_PROGRAMS = @CUPS_BIN@
cupsexec_backend_PROGRAMS = @CUPS_BACKEND_BIN@
cupsexec_filter_PROGRAMS = @CUPS_FILTER_BIN@
noinst_PROGRAMS = @CUPS_NOINST_BIN@

EXTRA_PROGRAMS = cups-calibrate commandtoepson epson genppd rastertoprinter commandtocanon canon

CUPS_LIBS = @CUPS_LIBS@

cups_calibrate_SOURCES = cups-calibrate.c
cups_calibrate_LDADD = $(LIBPRINTUT) -lm

commandtoepson_SOURCES = commandtoepson.c
commandtoepson_LDADD = $(LIBPRINTUT) $(CUPS_LIBS)

canon_SOURCES = canon.c
canon_LDADD = $(LIBPRINTUT) $(CUPS_LIBS)

epson_SOURCES = epson.c
epson_LDADD = $(LIBPRINTUT) $(CUPS_LIBS)

genppd_SOURCES = genppd.c
genppd_LDADD = $(LIBPRINTUT) $(CUPS_LIBS) $(LIBGIMPPRINT_LIBS)
genppd_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)

rastertoprinter_SOURCES = rastertoprinter.c
rastertoprinter_LDADD = $(LIBPRINTUT) $(CUPS_LIBS) $(LIBGIMPPRINT_LIBS)
rastertoprinter_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)


## Data

CUPS_PKG = calibrate.ppm
CUPS_CONF = command.types

pkgdata_DATA = @CUPS_PKG@
pkgsysconf_DATA = @CUPS_CONF@

if BUILD_CUPS
INSTALL_DATA_LOCAL_DEPS = ppd
endif

if BUILD_TRANSLATED_PPDS
PPD_NLS = $(PPD_NLS_1)
LINGUAS = $(ALL_LINGUAS)
endif

if USE_NLS
PPD_NLS_1 = ppd-nls
endif

install-data-local: $(INSTALL_DATA_LOCAL_DEPS)
	if test -n "$(INSTALL_DATA_LOCAL_DEPS)" ; then \
	  $(mkinstalldirs) $(DESTDIR)$(cups_modeldir); \
	  cd ppd ; \
	  for language in * ; do \
	    cd ..; \
	    $(mkinstalldirs) $(DESTDIR)$(cups_modeldir)/$$language; \
	    cd ppd/$$language; \
	    for ppdfile in * ; do \
	      (cd ../..; $(INSTALL_DATA) ppd/$$language/$$ppdfile $(DESTDIR)$(cups_modeldir)/$$language) ; \
	    done; \
	  cd ..; \
	  done \
	fi

uninstall-local: $(INSTALL_DATA_LOCAL_DEPS)
	if test -n "$(INSTALL_DATA_LOCAL_DEPS)" ; then \
	  for language in C $(LINGUAS) ; do \
	    echo "Removing $$language locale PPD files..." ; \
	    for ppdfile in ppd/$$language/* ; do \
	      if test -f "$(DESTDIR)$(cups_modeldir)/$$language/`basename $$ppdfile`" ; then \
	        $(RM) -f $(DESTDIR)$(cups_modeldir)/$$language/`basename $$ppdfile` ; \
	      fi ; \
	    done; \
	  done \
	fi

refresh-data-local: ppd
	cd ppd ; \
	files=`find . -name '*.ppd*' -exec basename '{}' \; | sort | uniq` ; \
	for language in . de en es fr it ; do \
	  for f in $$files ; do \
	    ff="$(DESTDIR)/$(cups_modeldir)/$$language/$$f" ; \
	    if [ -f "$$ff" ] ; then \
	      echo "Removing $$ff" ; \
	      $(RM) "$$ff" ; \
	    fi ; \
	    if [ -f "$$ff" ] ; then \
	      echo "Unable to remove $$ff" 1>&2 ; \
	      exit 1 ; \
	    fi ; \
	  done; \
	done


install-data-hook:
# Remove unused directories in install tree
	-@echo 'Expect a number of "rmdir: Directory not empty" warnings'
	-@echo 'These messages are harmless and should be ignored.'
	-rmdir $(DESTDIR)$(cups_modeldir)
	-rmdir $(DESTDIR)$(pkgdatadir)
	-rmdir $(DESTDIR)$(cupsexec_backenddir)
	-rmdir $(DESTDIR)$(cupsexec_filterdir)
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(pkglibdir)
	-rmdir $(DESTDIR)$(pkgsysconfdir)
	-rmdir `dirname $(DESTDIR)$(pkgdatadir)`
	-rmdir `dirname $(DESTDIR)$(pkglibdir)`
	-rmdir `dirname $(DESTDIR)$(pkgsysconfdir)`


## Rules

.PHONY: ppd ppd-stamp-pre ppd-stamp-nonls ppd-stamp-nls ppd-stamp-phony

all-local: $(INSTALL_DATA_LOCAL_DEPS)

ppd: ppd-stamp

ppd-stamp: genppd
	$(MAKE) ppd-stamp-phony
	touch ppd-stamp

ppd-stamp-phony: ppd-stamp-pre ppd-nonls $(PPD_NLS)

ppd-stamp-pre:
	-$(RM) -r ppd
	-mkdir ppd

ppd-nonls: genppd
	for language in C ; do \
	  mkdir ppd/$$language ; \
	  echo "$$language:" ; \
	 ./genppd --prefix=ppd/$$language ; \
	done

ppd-nls: genppd
	wdir=`pwd` ; \
	cd $(top_builddir)/po ; \
	$(MAKE) ; \
	$(MAKE) DESTDIR= prefix=$${wdir}/catalog datadir=$${wdir}/catalog/share install
	for language in $(LINGUAS) ; do \
	  mkdir ppd/$$language ; \
	  echo "$$language:" ; \
	  LANGUAGE=$$language ./genppd --prefix=ppd/$$language --catalog=`pwd`/catalog/share/locale ; \
	done
	$(RM) -r catalog

$(LIBGIMPPRINT_LIBS):
	cd ../main ; \
	$(MAKE)

dist-hook:
	for dir in commands ; do \
	  if test -d $(srcdir)/$$dir ; then \
	    mkdir $(distdir)/$$dir; \
	    for dirfile in $(srcdir)/$$dir/*; do \
	      if test -f $$dirfile ; then \
	        cp -p $$dirfile $(distdir)/$$dir; \
	      fi \
	    done \
	  fi \
	done

clean-local:
	-$(RM) -r ppd


## Clean

CLEANFILES = $(EXTRA_PROGRAMS) ppd-stamp
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = \
	COPYING \
	README \
	calibrate.ppm \
	command.txt \
	command.types

