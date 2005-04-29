## $Id: Makefile.am,v 1.1.1.1 2004/07/23 06:26:25 jlovell Exp $
## Copyright (C) 2001 Andy Stewart and Roger Leigh
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

MANUAL_SRC_FILES = \
	copying.xml \
	dither.xml \
	escp2.xml \
	gimpprint.xml \
	gpl-appendix.xml \
	introduction.xml \
	new-printer.xml \
	problems.xml \
	using.xml \
	weave.xml

HTMLDIR = reference-html

if INSTALL_USER_GUIDE
STANDARD_TARGETS = html
endif

DISTHOOKDIRS = $(HTMLDIR)


if MAINTAINER_MODE
MAINT_MODE = true
else
MAINT_MODE = false
endif

if MAINTAINER_MODE
ALL_LOCAL_TARGETS = docs
else
ALL_LOCAL_TARGETS = $(STANDARD_TARGETS)
endif

all-local: $(ALL_LOCAL_TARGETS)


## Rules

dist-hook: html-stamp
	for dir in $(DISTHOOKDIRS) ; do \
	  if test -d $(srcdir)/$$dir ; then \
	    mkdir $(distdir)/$$dir; \
	    for dirfile in $(srcdir)/$$dir/*; do \
	      if test -f $$dirfile ; then \
	        cp -p $$dirfile $(distdir)/$$dir ; \
	      fi ; \
	      if test -d $$dirfile ; then \
	        cp -pR $$dirfile $(distdir)/$$dir ; \
	      fi ; \
	    done \
	  fi \
	done
	for file in $(DISTHOOKFILES) ; do \
	  if test -f $$file ; then \
	    cp -p $$file $(distdir) ; \
	fi ; \
	done

docs: html pdf

pdf: gimpprint.pdf

gimpprint.pdf:  $(MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  for file in $(MANUAL_SRC_FILES) ; do \
	    if test -L $$file ; then \
	      $(RM) -f $$file ; \
	    fi ; \
	    $(LN_S) $(srcdir)/$$file $$file ; \
	  done ; \
	fi ; \
	$(DB2PDF) gimpprint.xml

html: html-stamp

# This ugly workaround with SOURCE is because db2html doesn't seem to like
# "./users-guide.sgml".  Note that db2ps does not have the same limitation.
# html-stamp should *only* be called by html--it's just a timestamp!

html-stamp: $(MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  for file in $(MANUAL_SRC_FILES) ; do \
	    if test -L $$file ; then \
	      $(RM) -f $$file ; \
	    fi ; \
	    $(LN_S) $(srcdir)/$$file $$file ; \
	  done ; \
	fi
	$(DB2HTML) gimpprint.xml
	-$(RM) -rf $(HTMLDIR)
	mv gimpprint $(HTMLDIR)
	chmod a+rx $(HTMLDIR)
	touch html-stamp

install-data-local: $(STANDARD_TARGETS)
	if test -n '$(STANDARD_TARGETS)' ; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/doc/$(HTMLDIR) ; \
	  if test -f gimpprint.pdf ; then \
	    $(INSTALL_DATA) gimpprint.pdf $(DESTDIR)$(datadir)/$(PACKAGE)/doc ; \
	  elif test -f $(srcdir)/gimpprint.pdf ; then \
	    $(INSTALL_DATA) $(srcdir)/gimpprint.pdf $(DESTDIR)$(datadir)/$(PACKAGE)/doc ; \
	  fi ; \
	  if test -d $(HTMLDIR) ; then \
	  HTMLGENDIR="$(HTMLDIR)" ; \
	  elif test -d $(srcdir)/$(HTMLDIR) ; then \
	    HTMLGENDIR="$(srcdir)/$(HTMLDIR)" ; \
	  else \
	    exit 1 ; \
	  fi ; \
	  for file in $$HTMLGENDIR/*.html $$HTMLGENDIR/*.css ; do \
	    if test -f $$file ; then \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(datadir)/$(PACKAGE)/doc/$(HTMLDIR) ; \
	    fi ; \
	  done ; \
	fi

uninstall-local:
	-$(RM) -r $(DESTDIR)$(datadir)/$(PACKAGE)/doc

CLEAN_MANUALS = \
	$(RM) -f gimpprint.pdf gimpprint.ps;	\
	$(RM) -rf *html-stamp $(HTMLDIR)

clean-local:
	if test $(srcdir) = "." -a $(MAINT_MODE) = false ; then \
	  echo "clean-local: Not removing $(HTMLDIR)" ; \
	  echo 'clean-local: Not removing gimpprint.pdf' ; \
	  echo 'clean-local: Not removing gimpprint.ps' ; \
	else \
	  $(CLEAN_MANUALS) ; \
	fi
	if test $(srcdir) = "." ; then \
	  : ; \
	else \
	  $(RM) -f $(MANUAL_SRC_FILES) ; \
	fi
	-$(RM) -rf *.tex *.log *.aux *.dvi *.gz *.out *.junk *.out *.fot
	-$(RM) -rf db2html* DB2HTML*

veryclean:
	$(MAKE) MAINT_MODE=true clean

maintainer-clean-local:
	$(CLEAN_MANUALS)


## Clean

MAINTAINERCLEANFILES = Makefile.in *html-stamp


EXTRA_DIST = \
	$(MANUAL_SRC_FILES) \
	gimpprint.pdf \
	html-stamp

.PHONY: docs html ps pdf
