## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.1.1.3 2004/07/23 06:26:24 jlovell Exp $
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
ACLOCAL_AMFLAGS = -I m4 -I m4local

include $(top_srcdir)/scripts/global.mk


## Variables

SUBDIRS = include src samples test po man doc debian scripts
DISTDIRS = m4 m4extra


## Rules

# Libtool dependency
libtool: $(LIBTOOL_DEPS)
	cd $(srcdir); \
	$(SHELL) ./config.status --recheck

# This rather strange-looking code ensures that the change log is only
# rebuilt if any of the CVS/Entries files have changed since the change log
# was last built.

ChangeLogPhony:
	@cd $(top_srcdir) ; \
	for f in `find . -name Entries -print | grep CVS` ; do \
	  if /usr/bin/test ! -f ChangeLogStamp -o ChangeLogStamp -ot $$f ; then \
	    rm -f ChangeLogStamp ; \
	    touch ChangeLogStamp ; \
	  fi ; \
	done

ChangeLogStamp: ChangeLogPhony
	@/bin/true

ChangeLog: ChangeLogStamp
	@cd $(top_srcdir) ; \
	if test -f $(top_builddir)/.noupdate ; then \
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
	  echo "ChangeLog: This is not a CVS version of Gimp-Print" ; \
	  echo "ChangeLog: Not updating ChangeLog from CVS!" ; \
	else \
	  echo "ChangeLog: No ChangeLog or ChangeLog of zero size found!" ; \
	  echo "ChangeLog: Failed to update ChangeLog ['touch .noupdate' to skip]" ; \
	  exit 1 ; \
	fi

configure.in:
	$(LN_S) $(srcdir)/configure.ac $(srcdir)/configure.in

dist-time-check:
	@current_date=`date +%Y%m%d`; \
	configure_date=`ls -l --time-style=+%Y%m%d $(top_builddir)/configure | awk '{print $$6}'`; \
	echo "Checking that 'configure' was built today."; \
	if test "$$current_date" != "$$configure_date" ; then \
	  echo "configure is out of date -- rerun ./autogen.sh first!"; \
	  exit 1; \
	fi

dist-hook: dist-time-check
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
# Remove configure.in (libtool bug workaround)
	$(RM) -f $(distdir)/configure.in
# Remove any backup files found in distdir
	$(FIND) $(distdir) -name '.*' -print | xargs $(RM) -rf
	$(FIND) $(distdir) -name '#*' -print | xargs $(RM) -rf
	$(FIND) $(distdir) -name '*~' -print | xargs $(RM) -rf

install-cups: all
	cd src/cups ; \
	$(MAKE) install

install-ghost: all
	cd src/ghost ; \
	$(MAKE) install

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
	depcomp \
	install-sh \
	ltconfig \
	mdate-sh \
	missing \
	mkinstalldirs

CLEANMFOUR = \
	codeset.m4 \
	gettext.m4 \
	glibc21.m4 \
	iconv.m4 \
	intdiv0.m4 \
	inttypes_h.m4 \
	inttypes.m4 \
	inttypes-pri.m4 \
	isc-posix.m4 \
	lcmessage.m4 \
	lib-ld.m4 \
	lib-link.m4 \
	lib-prefix.m4 \
	progtest.m4 \
	stdint_h.m4 \
	uintmax_t.m4 \
	ulonglong.m4

CLEANPO = \
	boldquot.sed \
	en@boldquot.header \
	en@quot.header \
	insert-header.sin \
	Makevars.template \
	quot.sed \
	remove-potcdate.sin \
	Rules-quot

CLEANFILES = .noupdate

MAINTAINERCLEANFILES = \
	ABOUT-NLS \
	INSTALL \
	aclocal.m4 \
	config.h.in \
	configure \
	configure.in \
	Makefile.in \
	stamp-h.in \
	stamp-h1.in \
	stamp-h2.in \
	stamp-h3.in \
	stamp-h4.in \
	$(addprefix scripts/, $(CLEANSCRIPTS)) \
	$(addprefix m4/, $(CLEANMFOUR)) \
	$(addprefix po/, $(CLEANPO))

EXTRA_DIST = autogen.sh ChangeLogStamp

.PHONY: deb html install-cups install-gimp install-ghost snapshot ChangeLogPhony dist-time-check
