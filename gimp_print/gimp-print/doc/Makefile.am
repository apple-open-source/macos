## $Id: Makefile.am,v 1.1.1.2 2004/07/23 06:26:25 jlovell Exp $
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

SUBDIRS = developer users_guide

include $(top_srcdir)/scripts/global.mk


## Variables

docdir = $(datadir)/doc/$(PACKAGE)

## Rules

if MAINTAINER_MODE
all-local: gimpprint-stamp
else
all-local:
endif

# Rerun doxygen whenever autogen is run
gimpprint-stamp: gimpprint.dox.in $(top_srcdir)/configure
	$(RM) -rf gimpprint/html
	$(DOXYGEN) gimpprint.dox
	touch $@

docs: gimpprint-stamp
	cd developer; $(MAKE)
	cd users_guide; $(MAKE)

ps: $(MANUAL).ps
	cd developer; $(MAKE) ps
	cd users_guide; $(MAKE) ps

pdf: $(MANUAL).pdf
	cd developer; $(MAKE) pdf
	cd users_guide; $(MAKE) pdf

html: 
	cd developer; $(MAKE) html
	cd users_guide; $(MAKE) html


## Clean

maintainer-clean-local:
	$(RM) -r gimpprint

MAINTAINERCLEANFILES = \
	Makefile.in \
	gimpprint-stamp

EXTRA_DIST = \
	FAQ.html \
	README.maintaining \
	gimpprint.dox.in \
	gimpprint \
	gimpprint-stamp

.PHONY: html ps pdf docs
