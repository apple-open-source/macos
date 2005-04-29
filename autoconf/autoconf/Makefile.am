## Process this file with automake to create Makefile.in. -*-Makefile-*-

## Makefile for Autoconf.
## Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

# bin/ and tests/ must be run first, as they build the tests executables
# (tests/autom4te etc.), that we happen to use here.
SUBDIRS = bin tests . lib config man doc

ACLOCAL_AMFLAGS = -I config

EXTRA_DIST = ChangeLog.0 ChangeLog.1 ChangeLog.2 \
	     BUGS \
	     GNUmakefile Makefile.maint Makefile.cfg

MAINTAINERCLEANFILES = INSTALL

## -------------------- ##
## Forwarding targets.  ##
## -------------------- ##

html:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) $@

.PHONY: html


## --------- ##
## INSTALL.  ##
## --------- ##

pkgdata_DATA = INSTALL
AM_MAKEINFOFLAGS = --no-headers --no-validate --no-split
INSTALL: $(top_srcdir)/doc/install.texi
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -o $(srcdir)/INSTALL \
	  $(top_srcdir)/doc/install.texi

## maintainer-check ##
maintainer-check: maintainer-check-tests
maintainer-check-tests:
	cd tests && make maintainer-check


## ----------------------------------- ##
## Updating Perl files from Automake.  ##
## ----------------------------------- ##

## Fetch the latest versions of files we care about.
automake_cvsweb = \
   http://sources.redhat.com/cgi-bin/cvsweb.cgi/~checkout~/automake
automake_cvsargs = \
   'content-type=text/plain&cvsroot=automake'
autom4te_files = \
  Autom4te/Configure_ac.pm \
  Autom4te/Channels.pm \
  Autom4te/FileUtils.pm \
  Autom4te/Struct.pm \
  Autom4te/XFile.pm

autom4te-update:
	rm -rf Fetchdir > /dev/null 2>&1
	mkdir Fetchdir; mkdir Fetchdir/Autom4te
## If a get fails then that is a problem.
	(cd Fetchdir && \
	$(WGET) $(automake_cvsweb)/lib/Automake/Configure_ac.pm?$(automake_cvsargs) -O Autom4te/Configure_ac.pm; \
	$(WGET) $(automake_cvsweb)/lib/Automake/Channels.pm?$(automake_cvsargs) -O Autom4te/Channels.pm; \
	$(WGET) $(automake_cvsweb)/lib/Automake/FileUtils.pm?$(automake_cvsargs) -O Autom4te/FileUtils.pm; \
	$(WGET) $(automake_cvsweb)/lib/Automake/Struct.pm?$(automake_cvsargs) -O Autom4te/Struct.pm; \
	$(WGET) $(automake_cvsweb)/lib/Automake/XFile.pm?$(automake_cvsargs) -O Autom4te/XFile.pm; \
	:)
	perl -pi -e 's/Automake::/Autom4te::/g' Fetchdir/Autom4te/*.pm
	for file in $(autom4te_files); do \
	  $(move_if_change) Fetchdir/$$file $(srcdir)/lib/$$file; \
	done
