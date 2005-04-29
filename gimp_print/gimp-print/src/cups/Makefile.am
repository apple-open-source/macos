## $Id: Makefile.am,v 1.1.1.4 2004/12/22 23:49:34 jlovell Exp $
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

include $(top_srcdir)/scripts/global.mk


## Variables

empty=
PACKAGE = cups

bindir = @cups_bindir@
sbindir = @cups_sbindir@

pkgdatadir = $(cups_conf_datadir)
pkglibdir = $(cups_conf_serverbin)
pkgsysconfdir = $(cups_conf_serverroot)

cupsexec_backenddir = $(pkglibdir)/backend
cupsexec_filterdir = $(pkglibdir)/filter
if CUPS_PPDS_AT_TOP_LEVEL
cups_modeldir = $(pkgdatadir)/model/
DONT_UNINSTALL_PPDS=true
else
cups_modeldir = $(pkgdatadir)/model/gimp-print/@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@/
endif

LOCAL_CPPFLAGS = $(GIMPPRINT_CFLAGS) $(CUPS_CFLAGS)


## Programs

if BUILD_CUPS
bin_PROGRAMS = cups-calibrate
sbin_PROGRAMS = cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@
sbin_SCRIPTS = cups-genppdconfig.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ cups-genppdupdate.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@
cupsexec_backend_PROGRAMS = epson canon
cupsexec_filter_PROGRAMS = rastertogimpprint.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ commandtoepson commandtocanon
endif

if BUILD_GENPPD_STATIC
STATIC_LDOPTS=-static -export-dynamic
endif

cups_calibrate_SOURCES = cups-calibrate.c
cups_calibrate_LDADD = -lm

commandtoepson_SOURCES = commandtoepson.c
commandtoepson_LDADD = $(CUPS_LIBS)

canon_SOURCES = canon.c
canon_LDADD = $(CUPS_LIBS)

epson_SOURCES = epson.c
epson_LDADD = $(CUPS_LIBS)

cups_genppd_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_SOURCES = genppd.c
cups_genppd_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_LDADD = $(CUPS_LIBS) $(GENPPD_LIBS) $(GIMPPRINT_LIBS)
cups_genppd_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_LDFLAGS = $(STATIC_LDOPTS)

rastertogimpprint_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_SOURCES = rastertoprinter.c
rastertogimpprint_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_LDADD = $(CUPS_LIBS) $(GIMPPRINT_LIBS)
rastertogimpprint_@GIMPPRINT_MAJOR_VERSION@_@GIMPPRINT_MINOR_VERSION@_LDFLAGS = $(STATIC_LDOPTS)

cups-genppdconfig.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@: $(top_builddir)/config.status cups-genppdconfig.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/cups-genppdconfig
cups-genppdupdate.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@: $(top_builddir)/config.status cups-genppdupdate.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/cups-genppdupdate

## Data

if BUILD_CUPS
CUPS_PKG = calibrate.ppm
endif
if BUILD_CUPS
CUPS_CONF = command.types
endif

pkgdata_DATA = $(CUPS_PKG)
pkgsysconf_DATA = $(CUPS_CONF)

if BUILD_CUPS_PPDS
INSTALL_DATA_LOCAL_DEPS = ppd-stamp
endif

if BUILD_TRANSLATED_CUPS_PPDS
PPD_NLS = $(PPD_NLS_1)
LINGUAS = $(ALL_LINGUAS)
endif

if USE_NLS
PPD_NLS_1 = ppd-nls
endif


## Rules

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
# Remove PPDs installed in new PPD location
	if test -z "DONT_UNINSTALL_PPDS" ; then \
	  if test -n "$(INSTALL_DATA_LOCAL_DEPS)" ; then \
	    $(RM) -fr $(DESTDIR)$(cups_modeldir)/* ; \
	  fi \
	fi

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

.PHONY: ppd ppd-stamp-pre ppd-stamp-nonls ppd-stamp-nls ppd-stamp-phony

all-local: $(INSTALL_DATA_LOCAL_DEPS)

ppd: ppd-stamp

ppd-stamp: cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ $(top_srcdir)/src/main/printers.xml $(top_srcdir)/src/main/papers.xml
	$(MAKE) ppd-stamp-phony
	touch ppd-stamp

ppd-stamp-phony: ppd-stamp-pre ppd-nonls $(PPD_NLS)

ppd-stamp-pre:
	-$(RM) -r ppd
	-mkdir ppd

ppd-nonls: cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@
	for language in C ; do \
	  mkdir ppd/$$language ; \
	  echo "$$language:" ; \
	 ./cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ -p ppd/$$language ; \
	done

ppd-nls: cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@
	wdir=`pwd` ; \
	cd $(top_builddir)/po ; \
	$(MAKE) ; \
	$(MAKE) DESTDIR= prefix=$${wdir}/catalog datadir=$${wdir}/catalog/share install
	for language in `./cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ -L -c \`pwd\`/catalog/share/locale` ; do \
	  mkdir ppd/$$language ; \
	  echo "$$language:" ; \
	  LANGUAGE=$$language ./cups-genppd.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ -p ppd/$$language -c `pwd`/catalog/share/locale ; \
	done
	$(RM) -r catalog

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

CLEANFILES = ppd-stamp
DISTCLEANFILES = cups-genppdconfig.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@ cups-genppdupdate.@GIMPPRINT_MAJOR_VERSION@.@GIMPPRINT_MINOR_VERSION@
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = \
	COPYING \
	README \
	calibrate.ppm \
	command.txt \
	command.types

