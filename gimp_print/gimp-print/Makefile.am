## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1.1.1 2003/01/27 19:05:31 jlovell Exp $
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

NONGIMP_SUBDIRS =
NONGIMP_DISTDIRS =
## @-@-@-@-@ NO-GIMP @-@-@-@-@
NONGIMP_SUBDIRS = test
NONGIMP_DISTDIRS = debian
## @-@-@-@-@ YES-GIMP @-@-@-@-@
SUBDIRS = include intl lib man src samples $(NONGIMP_SUBDIRS) po doc
DISTDIRS = scripts $(NONGIMP_DISTDIRS)

EXTRA_DIST = configure.in.in autogen.sh

if MAINTAINER_MODE
MAINT_MODE = true
endif


## Rules

## Libtool dependency
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

## @-@-@-@-@ NO-GIMP @-@-@-@-@
ChangeLog:
	@cd $(top_srcdir) ; \
	echo `pwd`
	if test -f .noupdate ; then \
	  echo "ChangeLog: .noupdate found: NOT updating ChangeLog from CVS" ; \
	elif test -d CVS ; then \
	  echo "ChangeLog: Updating ChangeLog from CVS" ; \
	  cvs log -N | $(PERL) scripts/mkchlog > ChangeLog.tmp ; \
	  if test -s ChangeLog.tmp ; then \
	    mv ChangeLog.tmp ChangeLog ; \
	  else \
	    echo "ChangeLog: Failed to update ChangeLog ['touch .noupdate' to skip]" ; \
	    $(RM) ChangeLog.tmp ; \
	    exit 1 ; \
	  fi \
	elif test -s ChangeLog ; then \
	  echo "ChangeLog: This is not a CVS version of GIMP-Print" ; \
	  echo "ChangeLog: Not updating ChangeLog from CVS!" ; \
	else \
	  echo "ChangeLog: No ChangeLog or ChangeLog of zero size found!" ; \
	  echo "ChangeLog: Failed to update ChangeLog ['touch .noupdate' to skip]" ; \
	  exit 1 ; \
	fi

## @-@-@-@-@ YES-GIMP @-@-@-@-@

# Rebuild configure.in from configure.in.in if configure.in.in is available.
# If not, don't do anything.

configure.in: configure.in.in
	if test -f configure.in.in ; then \
	  sed "s/XXXRELEASE_DATE=XXX/RELEASE_DATE=\"`date '+%d %b %Y'`\"/" configure.in.in > configure.in ; \
	fi

configure.in.in:
	true

dist-hook:
	for dir in $(DISTDIRS) ; do \
	  if test -d $(srcdir)/$$dir ; then \
	    mkdir $(distdir)/$$dir; \
	    for dirfile in $(srcdir)/$$dir/*; do \
	      if test -f $$dirfile ; then \
	        cp -p $$dirfile $(distdir)/$$dir; \
	      fi \
	    done \
	  fi \
	done
## @-@-@-@-@ NO-GIMP @-@-@-@-@
# Create debian/control
	DEBIAN_VERSION=`grep "(" debian/changelog | head -1l | sed "s/^[^(]*(//" | sed "s/).*$$//"` ; \
	UPSTREAM_VERSION=`echo $$DEBIAN_VERSION | sed "s/-[^-]*$$//"` ; \
	LIBRARY_VERSION=$$(($(LT_CURRENT) - $(LT_AGE))) ; \
	sed -e "s/#UPSTREAM_VERSION#/$${UPSTREAM_VERSION}/g" -e "s/#LIBRARY_VERSION#/$${LIBRARY_VERSION}/g" <debian/control.in >$(distdir)/debian/control
# Do some extra magic to make a distribution for GIMP CVS
	status=0 ; \
	if test -f .gimp-dist ; then \
	  $(RM) -r $(distdir)/src/cups $(distdir)/src/foomatic $(distdir)/src/ghost ; \
	  $(RM) -r $(distdir)/debian $(distdir)/scripts/* $(distdir)/test ; \
	  $(RM) -r $(distdir)/intl $(distdir)/po/Makefile.in.in $(distdir)/po/*.gmo ; \
	  $(RM) -r $(distdir)/ABOUT-NLS $(distdir)/INSTALL $(distdir)/aclocal.m4 $(distdir)/config.h.in $(distdir)/configure $(distdir)/cups-gimpprint.list.in $(distdir)/stamp-h.in ; \
	  find $(distdir) -name Makefile.in -print | xargs --no-run-if-empty $(RM) ; \
	  cd $(distdir) ; \
	  $(PERL) ../scripts/patch-gimp configure.in ; \
	  $(PERL) ../scripts/patch-gimp Makefile.am ; \
	  $(PERL) ../scripts/patch-gimp src/Makefile.am ; \
	  $(PERL) ../scripts/patch-gimp po/POTFILES.in ; \
	  status=$$? ; \
	  cd .. ; \
	  $(RM) .gimp-dist ; \
	fi ; \
	exit $$status

gimp-dist:
	touch .gimp-dist
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-GIMP-DIST

install-cups: all
	cd src/cups ; \
	$(MAKE) install

install-ghost: all
	cd src/ghost ; \
	$(MAKE) install

## @-@-@-@-@ YES-GIMP @-@-@-@-@
install-gimp: all
	cd src/gimp ; \
	$(MAKE) install

docs:
	cd doc ; \
	$(MAKE) docs

html:
	cd doc ; \
	$(MAKE) html

ps:
	cd doc ; \
	$(MAKE) ps

pdf:
	cd doc ; \
	$(MAKE) pdf

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%Y%m%d"`

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

debian/control:
	debian/rules debian/control


## Clean

CLEANSCRIPTS = \
	config.guess \
	config.sub \
	install-sh \
	ltconfig \
	ltmain.sh \
	missing \
	mkinstalldirs

CLEANFILES = .gimp-dist .noupdate
MAINTAINERCLEANFILES = ABOUT-NLS INSTALL aclocal.m4 config.h.in configure Makefile.in stamp-h.in $(addprefix scripts/, $(CLEANSCRIPTS))

## @-@-@-@-@ NO-GIMP @-@-@-@-@
.PHONY: deb gimp-dist install-cups install-ghost snapshot ChangeLog
## @-@-@-@-@ YES-GIMP @-@-@-@-@

.PHONY: install-gimp html
