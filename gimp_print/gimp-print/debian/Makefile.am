## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1.1.1 2004/07/23 06:26:24 jlovell Exp $
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

AUTOMAKE_OPTIONS = 1.5 dist-bzip2 check-news gnu no-dependencies 

@SET_MAKE@

MAINT_CHARSET = latin1

include $(top_srcdir)/scripts/global.mk


## Variables

EXTRA_DIST = \
	changelog \
	compat \
	control.in \
	copyright \
	cupsys-driver-gimpprint.docs \
	cupsys-driver-gimpprint.install \
	cupsys-driver-gimpprint.postinst \
	cupsys-driver-gimpprint.postrm \
	escputil.docs \
	escputil.install \
	foomatic-db-gimp-print.install \
	gimp-print.docs \
	gimp-print.install \
	gimp-print.postrm \
	gimp-print.preinst \
	gimpprint-doc.doc-base \
	gimpprint-doc.install \
	gimpprint-locales.install \
	ijsgimpprint.install \
	libgimpprint-dev.docs \
	libgimpprint-dev.install \
	libgimpprint-doc.doc-base.developer.in \
	libgimpprint-doc.doc-base.reference.in \
	libgimpprint-doc.install.in \
	libgimpprint.install.in \
	README.building \
	README.Debian \
	rules


## Rules

dist-hook:
# Distribute debian/control (not in EXTRA_DIST to avoid $(srcdir) dependencies
	DEBIAN_VERSION=`grep "(" $(srcdir)/changelog | head -1l | sed "s/^[^(]*(//" | sed "s/).*$$//"` ; \
	UPSTREAM_VERSION=`echo $$DEBIAN_VERSION | sed "s/-[^-]*$$//"` ; \
	LIBRARY_VERSION="-$${UPSTREAM_VERSION}" ; \
	sed -e "s/#UPSTREAM_VERSION#/$${UPSTREAM_VERSION}/g" \
	    -e "s/#LIBRARY_VERSION#/$${LIBRARY_VERSION}/g" \
	    <$(srcdir)/control.in >$(distdir)/control

DEBUILD_OPTIONS = -us -uc
deb: debian/changelog debian/control debian/rules
	$(MAKE) dist
	-$(RM) -r debian-build
	-mkdir debian-build
	cd debian-build ; \
	mv ../$(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)_$(VERSION).orig.tar.gz ; \
	tar xfvz $(PACKAGE)_$(VERSION).orig.tar.gz ; \
	cd $(PACKAGE)-$(VERSION) ; \
	debuild $(DEBUILD_OPTIONS) ; \
	cd .. ; \
	$(RM) -r $(PACKAGE)-$(VERSION)

control:
	cd ..; \
	debian/rules debian/control


## Clean

MAINTAINERCLEANFILES = \
	Makefile.in


.PHONY: deb
