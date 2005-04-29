## $Id: Makefile.am,v 1.1.1.3 2004/07/23 06:26:26 jlovell Exp $
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

EPSLEVEL = 2

UTIL_MANUAL = util-edition
UTIL_MANUAL_SRC_FILES = $(srcdir)/$(UTIL_MANUAL).sgml \
	$(srcdir)/title.sgml \
	$(srcdir)/history.sgml \
	$(srcdir)/utils.sgml \
	$(srcdir)/gfdl.sgml

GIMP_MANUAL = gimp-edition
GIMP_MANUAL_SRC_FILES = $(srcdir)/$(GIMP_MANUAL).sgml \
	$(srcdir)/title.sgml \
	$(srcdir)/history.sgml \
	$(srcdir)/gimp.sgml \
	$(srcdir)/gfdl.sgml

CUPS_MANUAL = cups-edition
CUPS_MANUAL_SRC_FILES = $(srcdir)/$(CUPS_MANUAL).sgml \
	$(srcdir)/title.sgml \
	$(srcdir)/history.sgml \
	$(srcdir)/cups.sgml \
	$(srcdir)/gfdl.sgml

MANUAL = users-guide
MANUAL_SRC_FILES = $(srcdir)/$(MANUAL).sgml \
	$(srcdir)/title.sgml \
	$(srcdir)/history.sgml \
	$(srcdir)/gimp.sgml \
	$(srcdir)/cups.sgml \
	$(srcdir)/utils.sgml \
	$(srcdir)/gfdl.sgml

UTIL_HTMLDIR = util_html
GIMP_HTMLDIR = gimp_html
CUPS_HTMLDIR = cups_html
HTMLDIR = html

UTIL_OUTPUTS = \
 $(UTIL_MANUAL).pdf \
 util_html

GIMP_OUTPUTS = \
 $(GIMP_MANUAL).pdf \
 gimp_html

CUPS_OUTPUTS = \
 $(CUPS_MANUAL).pdf \
 cups_html

OUTPUTS = \
 $(MANUAL).pdf \
 html

UTIL_GUIDE = $(UTIL_MANUAL).pdf util_html
GIMP_GUIDE = $(GIMP_MANUAL).pdf gimp_html
CUPS_GUIDE = $(CUPS_MANUAL).pdf cups_html
USER_GUIDE = $(MANUAL).pdf html

if INSTALL_USER_GUIDE
STANDARD_TARGETS = $(USER_GUIDE)
endif

DISTHOOKDIRS = $(HTMLDIR)/stylesheet-images

UTIL_PNG_IMAGES =

CUPS_PNG_IMAGES = \
	figures/cups_admin.png				\
	figures/cups_admin_device.png			\
	figures/cups_admin_make.png			\
	figures/cups_admin_model.png			\
	figures/cups_admin_success.png			\
	figures/cups_config_printer.png			\
	figures/cups_my_printer.png			\
	figures/cups_printers.png			\
	figures/cups_startup.png

GIMP_PNG_IMAGES = \
	figures/gimp-print-gui-1.png			\
	figures/gimp-print-gui.png			\
	figures/gimp-print-new-printer.png		\
	figures/gimp-print-print-color-adjust.png	\
	figures/gimp-print-setup.png			\
	figures/gimp_image.png				\
	figures/gimp_startup.png

PNG_IMAGES = \
	$(CUPS_PNG_IMAGES) 				\
	$(GIMP_PNG_IMAGES)

DISTHOOKFILES = $(PNG_IMAGES) $(STANDARD_TARGETS) $(HTMLDIR)/*.html $(HTMLDIR)/*.css

UTIL_EPS_IMAGES = $(addsuffix .eps,$(basename $(UTIL_PNG_IMAGES)))
GIMP_EPS_IMAGES = $(addsuffix .eps,$(basename $(GIMP_PNG_IMAGES)))
CUPS_EPS_IMAGES = $(addsuffix .eps,$(basename $(CUPS_PNG_IMAGES)))
EPS_IMAGES = $(addsuffix .eps,$(basename $(PNG_IMAGES)))

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

## Rules

dist-hook: $(USER_GUIDE)
	for file in $(DISTHOOKFILES) ; do \
	  if test -f $$file ; then \
            if ! test -d $(distdir)/`dirname $$file` ; then \
	      mkdir $(distdir)/`dirname $$file` ; \
	    fi ; \
	    cp -p $$file $(distdir)/`dirname $$file` ; \
	  fi ; \
	done
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

$(UTIL_MANUAL).pdf:  $(UTIL_MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(UTIL_MANUAL_SRC_FILES) . ; \
	  if test -d figures ; then \
	    : ; \
	  else \
	    mkdir figures ; \
	  fi ; \
	fi ; \
	$(DB2PDF) $(UTIL_MANUAL).sgml

$(GIMP_MANUAL).pdf:  $(GIMP_MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(GIMP_MANUAL_SRC_FILES) . ; \
	  if test -d figures ; then \
	    : ; \
	  else \
	    mkdir figures ; \
	  fi ; \
	  for image in $(addprefix $(srcdir)/, $(GIMP_PNG_IMAGES)) ; do \
	    cp -p $$image figures/`basename $$image` ; \
	  done ; \
	fi ; \
	$(DB2PDF) $(GIMP_MANUAL).sgml

$(CUPS_MANUAL).pdf:  $(CUPS_MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(CUPS_MANUAL_SRC_FILES) . ; \
	  if test -d figures ; then \
	    : ; \
	  else \
	    mkdir figures ; \
	  fi ; \
	  for image in $(addprefix $(srcdir)/, $(CUPS_PNG_IMAGES)) ; do \
	    cp -p $$image figures/`basename $$image` ; \
	  done ; \
	fi ; \
	$(DB2PDF) $(CUPS_MANUAL).sgml

$(MANUAL).pdf:  $(MANUAL_SRC_FILES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(MANUAL_SRC_FILES) . ; \
	  if test -d figures ; then \
	    : ; \
	  else \
	    mkdir figures ; \
	  fi ; \
	  for image in $(addprefix $(srcdir)/, $(PNG_IMAGES)) ; do \
	    cp -p $$image figures/`basename $$image` ; \
	  done ; \
	fi ; \
	$(DB2PDF) $(MANUAL).sgml

editions:  docs cups_docs gimp_docs util_docs

util_docs: util_html util_pdf
gimp_docs: gimp_html gimp_pdf
cups_docs: cups_html cups_pdf
docs: html pdf

all_util_docs: util_html util_ps util_pdf
all_gimp_docs: gimp_html gimp_ps gimp_pdf
all_cups_docs: cups_html cups_ps cups_pdf
all_docs: html ps pdf

all-local: $(ALL_LOCAL_TARGETS)

util_pdf: $(UTIL_MANUAL).pdf
gimp_pdf: $(GIMP_MANUAL).pdf
cups_pdf: $(CUPS_MANUAL).pdf
pdf: $(MANUAL).pdf

util_ps: $(UTIL_MANUAL).ps
gimp_ps: $(GIMP_MANUAL).ps
cups_ps: $(CUPS_MANUAL).ps
ps: $(MANUAL).ps

$(UTIL_MANUAL).ps:  $(UTIL_MANUAL_SRC_FILES) $(UTIL_EPS_IMAGES)
	for i in $(UTIL_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		sed -e 's/\.png"/.eps"/g' \
		    -e 's/FORMAT="PNG"/FORMAT="EPS"/g' $$i > $$j-eps.sgml; \
	done
	cat $(UTIL_MANUAL).sgml | sed -e 's/\.sgml"/-eps.sgml"/g' > $(UTIL_MANUAL)-eps.sgml
	$(DB2PS) $(UTIL_MANUAL)-eps.sgml
	mv $(UTIL_MANUAL)-eps.ps $(UTIL_MANUAL).ps
	for i in $(UTIL_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		$(RM) -f $${j}-eps.*; \
	done

$(GIMP_MANUAL).ps:  $(GIMP_MANUAL_SRC_FILES) $(GIMP_EPS_IMAGES)
	for i in $(GIMP_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		sed -e 's/\.png"/.eps"/g' \
		    -e 's/FORMAT="PNG"/FORMAT="EPS"/g' $$i > $$j-eps.sgml; \
	done
	cat $(GIMP_MANUAL).sgml | sed -e 's/\.sgml"/-eps.sgml"/g' > $(GIMP_MANUAL)-eps.sgml
	$(DB2PS) $(GIMP_MANUAL)-eps.sgml
	mv $(GIMP_MANUAL)-eps.ps $(GIMP_MANUAL).ps
	for i in $(GIMP_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		$(RM) -f $${j}-eps.*; \
	done

$(CUPS_MANUAL).ps:  $(CUPS_MANUAL_SRC_FILES) $(CUPS_EPS_IMAGES)
	for i in $(CUPS_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		sed -e 's/\.png"/.eps"/g' \
		    -e 's/FORMAT="PNG"/FORMAT="EPS"/g' $$i > $$j-eps.sgml; \
	done
	cat $(CUPS_MANUAL).sgml | sed -e 's/\.sgml"/-eps.sgml"/g' > $(CUPS_MANUAL)-eps.sgml
	$(DB2PS) $(CUPS_MANUAL)-eps.sgml
	mv $(CUPS_MANUAL)-eps.ps $(CUPS_MANUAL).ps
	for i in $(CUPS_MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		$(RM) -f $${j}-eps.*; \
	done

$(MANUAL).ps:  $(MANUAL_SRC_FILES) $(EPS_IMAGES)
	for i in $(MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		sed -e 's/\.png"/.eps"/g' \
		    -e 's/FORMAT="PNG"/FORMAT="EPS"/g' $$i > $$j-eps.sgml; \
	done
	cat $(MANUAL).sgml | sed -e 's/\.sgml"/-eps.sgml"/g' > $(MANUAL)-eps.sgml
	$(DB2PS) $(MANUAL)-eps.sgml
	mv $(MANUAL)-eps.ps $(MANUAL).ps
	for i in $(MANUAL_SRC_FILES); do \
		j=`basename $$i .sgml`; \
		$(RM) -f $${j}-eps.*; \
	done

%.eps:
	if test ! -d figures ; then mkdir figures ; fi
	$(CONVERT) $(srcdir)/$(basename $@).png EPS$(EPSLEVEL):$@

util_html: util-html-stamp
gimp_html: gimp-html-stamp
cups_html: cups-html-stamp
html: html-stamp

# This ugly workaround with SOURCE is because db2html doesn't seem to like
# "./users-guide.sgml".  Note that db2ps does not have the same limitation.
# html-stamp should *only* be called by html--it's just a timestamp!

util-html-stamp: $(UTIL_MANUAL_SRC_FILES) $(UTIL_PNG_IMAGES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(UTIL_MANUAL_SRC_FILES) . ; \
	fi
	$(DB2HTML) $(UTIL_MANUAL).sgml
	-$(RM) -rf $(UTIL_HTMLDIR)
	mv $(UTIL_MANUAL) $(UTIL_HTMLDIR)
	chmod a+rx $(UTIL_HTMLDIR)
	touch util-html-stamp

gimp-html-stamp: $(GIMP_MANUAL_SRC_FILES) $(GIMP_PNG_IMAGES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(GIMP_MANUAL_SRC_FILES) . ; \
	fi
	$(DB2HTML) $(GIMP_MANUAL).sgml
	-mkdir $(GIMP_MANUAL)/figures
	for image in $(addprefix $(srcdir)/, $(GIMP_PNG_IMAGES)) ; do \
	  cp -p $$image $(GIMP_MANUAL)/figures/`basename $$image` ; \
	done
	-$(RM) -rf $(GIMP_HTMLDIR)
	mv $(GIMP_MANUAL) $(GIMP_HTMLDIR)
	chmod a+rx $(GIMP_HTMLDIR)
	touch gimp-html-stamp

cups-html-stamp: $(CUPS_MANUAL_SRC_FILES) $(CUPS_PNG_IMAGES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(CUPS_MANUAL_SRC_FILES) . ; \
	fi
	$(DB2HTML) $(CUPS_MANUAL).sgml
	-mkdir $(CUPS_MANUAL)/figures
	for image in $(addprefix $(srcdir)/, $(CUPS_PNG_IMAGES)) ; do \
	  cp -p $$image $(CUPS_MANUAL)/figures/`basename $$image` ; \
	done
	-$(RM) -rf $(CUPS_HTMLDIR)
	mv $(CUPS_MANUAL) $(CUPS_HTMLDIR)
	chmod a+rx $(CUPS_HTMLDIR)
	touch cups-html-stamp

html-stamp: $(MANUAL_SRC_FILES) $(PNG_IMAGES)
	if test $(srcdir) = '.' ; then \
	  : ; \
	else \
	  if test -L $(notdir $<) ; then \
	    $(RM) $(notdir $<) ; \
	  fi ; \
	  $(LN_S) $(MANUAL_SRC_FILES) . ; \
	fi
	$(DB2HTML) $(MANUAL).sgml
	-mkdir $(MANUAL)/figures
	for image in $(addprefix $(srcdir)/, $(PNG_IMAGES)) ; do \
	  cp -p $$image $(MANUAL)/figures/`basename $$image` ; \
	done
	-$(RM) -rf $(HTMLDIR)
	mv $(MANUAL) $(HTMLDIR)
	chmod a+rx $(HTMLDIR)
	touch html-stamp

install-data-local: $(STANDARD_TARGETS)
	if test -n '$(STANDARD_TARGETS)' ; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/doc/html/figures ; \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/doc/html/stylesheet-images ; \
	  if test -f $(MANUAL).pdf ; then \
	    $(INSTALL_DATA) $(MANUAL).pdf $(DESTDIR)$(datadir)/$(PACKAGE)/doc ; \
	  elif test -f $(srcdir)/$(MANUAL).pdf ; then \
	    $(INSTALL_DATA) $(srcdir)/$(MANUAL).pdf $(DESTDIR)$(datadir)/$(PACKAGE)/doc ; \
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
	      $(INSTALL_DATA) $$file $(DESTDIR)$(datadir)/$(PACKAGE)/doc/html ; \
	    fi ; \
	  done ; \
	  for file in $(srcdir)/figures/*.png ; do \
	    if test -f $$file ; then \
	      $(INSTALL_DATA) $$file $(DESTDIR)$(datadir)/$(PACKAGE)/doc/html/figures ; \
	    fi ; \
	  done ; \
	  if test -d $$HTMLGENDIR/stylesheet-images ; then \
	    for file in $$HTMLGENDIR/stylesheet-images/* ; do \
	      if test -f $$file ; then \
	        $(INSTALL_DATA) $$file $(DESTDIR)$(datadir)/$(PACKAGE)/doc/html/stylesheet-images ; \
	      fi ; \
	    done ; \
	  fi ; \
	fi

uninstall-local:
	-$(RM) -r $(DESTDIR)$(datadir)/$(PACKAGE)/doc

CLEAN_MANUALS = \
	$(RM) -f $(MANUAL).pdf $(MANUAL).ps ;	\
	$(RM) -f $(CUPS_MANUAL).pdf $(CUPS_MANUAL).ps ;	\
	$(RM) -f $(GIMP_MANUAL).pdf $(GIMP_MANUAL).ps ;	\
	$(RM) -f $(UTIL_MANUAL).pdf $(UTIL_MANUAL).ps ;	\
	$(RM) -rf *html-stamp $(HTMLDIR) $(CUPS_HTMLDIR) $(GIMP_HTMLDIR) $(UTIL_HTMLDIR) ;	\
	$(RM) -f $(EPS_IMAGES) $(CUPS_EPS_IMAGES) $(GIMP_EPS_IMAGES) $(UTIL_EPS_IMAGES)

clean-local:
	if test $(srcdir) = "." -a $(MAINT_MODE) = false ; then \
	  echo "clean-local: Not removing $(HTMLDIR)" ; \
	  echo 'clean-local: Not removing $(MANUAL).pdf' ; \
	  echo 'clean-local: Not removing $(MANUAL).ps' ; \
	  echo 'clean-local: Not removing EPS figures' ; \
	else \
	  $(CLEAN_MANUALS) ; \
	fi
	if test $(srcdir) = "." ; then \
	  : ; \
	else \
	  $(RM) -f $(MANUAL_SRC_FILES) $(CUPS_MANUAL_SRC_FILES) ; \
	  $(RM) -f $(GIMP_MANUAL_SRC_FILES) $(UTIL_CUPS_MANUAL_SRC_FILES) ; \
	  $(RM) -rf figures ; \
	fi
	-$(RM) -rf *.tex *.log *.aux *.dvi *.gz *.out *.junk *.out *.fot
	-$(RM) -rf db2html* DB2HTML*

veryclean:
	$(MAKE) MAINT_MODE=true clean

maintainer-clean-local:
	$(CLEAN_MANUALS)

#
# This is my (so far failed) attempt to create an index for this documentation.
#
#
#index.sgml:
#	$(PERL) /usr/share/sgml/docbook/docbook-dsssl-stylesheets-1.64/bin/collateindex.pl -N -o index.sgml
#
#	$(JADE)	-V html-index \
#		-t sgml \
#		-c /usr/share/sgml/CATALOG.jade_dsl \
#		-c /usr/share/sgml/CATALOG.docbook-dsssl-stylesheets \
#		-c /usr/share/sgml/CATALOG.gnome \
#		-c /usr/share/sgml/CATALOG.docbook_4 \
#		-c /usr/share/sgml/CATALOG.docbook_3  \
#		-D /usr/share/sgml/docbook/docbook-dsssl-stylesheets-1.64 \
#		-d /usr/share/sgml/docbook-toys/suse-both.dsl \
#		-o index.sgml \
#		gimp-print-users-guide-4.1.99-b3.sgml
#
#
# END failed attempt at index creation.
#

help:
	@echo "  "
	@echo "Targets for the complete user's guide:"
	@echo "  "
	@echo "    docs      - creates PS, PDF, and HTML documentation files"
	@echo "    all_docs  - creates PS and HTML documentation files"
	@echo "    ps        - creates PostScript documentation"
	@echo "    pdf       - creates PDF documentation"
	@echo "    html      - creates HTML documentation"
	@echo "    clean     - deletes machine generated files, but not outputs"
	@echo "    veryclean - deletes output files in addition"
	@echo "    help      - prints this help text"
	@echo ""
	@echo "Targets for specific manual editions:"
	@echo "  "
	@echo "    cups_html - generate HTML for CUPS edition of the manual"
	@echo "    cups_pdf  - generate PDF for CUPS edition of the manual"
	@echo "    cups_ps   - generate PS for CUPS edition of the manual"
	@echo "  "
	@echo "    gimp_docs - generate HTML, PDF, and PS for GIMP edition of the manual"
	@echo "    gimp_html - generate HTML for GIMP edition of the manual"
	@echo "    gimp_pdf  - generate PDF for GIMP edition of the manual"
	@echo "    gimp_ps   - generate PS for GIMP edition of the manual"
	@echo "  "
	@echo "    util_docs - generate HTML, PDF, and PS for UTIL edition of the manual"
	@echo "    util_html - generate HTML for UTIL edition of the manual"
	@echo "    util_pdf  - generate PDF for UTIL edition of the manual"
	@echo "    util_ps   - generate PS for UTIL edition of the manual"
	@echo "  "
	@echo "    editions  - generates HTML, PDF, and PS for ALL editions of the manual"
	@echo "  "


## Clean

MAINTAINERCLEANFILES = Makefile.in *html-stamp


EXTRA_DIST = README				\
	$(MANUAL_SRC_FILES)			\
	html-stamp				\
	cups-edition.sgml			\
	gimp-edition.sgml			\
	util-edition.sgml

.PHONY: docs cups_docs gimp_docs util_docs
.PHONY: all_docs all_cups_docs all_gimp_docs all_util_docs
.PHONY: html cups_html gimp_html util_html
.PHONY: ps cups_ps gimp_ps util_ps
.PHONY: pdf cups_pdf gimp_pdf util_pdf
.PHONY: help
