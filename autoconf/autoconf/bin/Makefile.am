## Process this file with automake to create Makefile.in. -*-Makefile-*-

## Makefile for Autoconf.
## Copyright (C) 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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

include ../lib/freeze.mk

bin_SCRIPTS = autom4te \
              autoconf autoheader autoreconf ifnames autoscan autoupdate

EXTRA_DIST = autoconf.in autoheader.in autoreconf.in autoupdate.in ifnames.in \
             autoscan.in autom4te.in \
	     autoconf.as

# Files that should be removed, but which Automake does not know.
CLEANFILES = $(bin_SCRIPTS)


## ------------- ##
## The scripts.  ##
## ------------- ##

edit = sed \
	-e 's,@SHELL\@,$(SHELL),g' \
	-e 's,@PERL\@,$(PERL),g' \
	-e 's,@bindir\@,$(bindir),g' \
	-e 's,@datadir\@,$(pkgdatadir),g' \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@autoconf-name\@,'`echo autoconf | sed '$(transform)'`',g' \
	-e 's,@autoheader-name\@,'`echo autoheader | sed '$(transform)'`',g' \
	-e 's,@autom4te-name\@,'`echo autom4te | sed '$(transform)'`',g' \
	-e 's,@M4\@,$(M4),g' \
	-e 's,@AWK\@,$(AWK),g' \
	-e 's,@VERSION\@,$(VERSION),g' \
	-e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g'

# autoconf is written in M4sh.
AUTOM4SH = $(top_builddir)/tests/autom4te --language M4sh --cache ''
$(srcdir)/autoconf.in: $(srcdir)/autoconf.as # FIXME: $(m4sh_m4f_dependencies)
	$(AUTOM4SH) $(srcdir)/autoconf.as -o $@

## All the files below depend on Makefile so that they are rebuilt
## when the prefix etc. changes.  It took quite a while to have these
## rules correct, don't break them!  In particular, we believe there is
## no solution to factor the code.
autoconf: $(srcdir)/autoconf.in Makefile
	rm -f autoconf autoconf.tmp
	$(edit) $(srcdir)/autoconf.in >autoconf.tmp
	chmod +x autoconf.tmp
	mv autoconf.tmp autoconf

autoheader: $(srcdir)/autoheader.in Makefile
	rm -f autoheader autoheader.tmp
	$(edit) $(srcdir)/autoheader.in >autoheader.tmp
	chmod +x autoheader.tmp
	mv autoheader.tmp autoheader

autom4te: $(srcdir)/autom4te.in Makefile
	rm -f autom4te autom4te.tmp
	$(edit) $(srcdir)/autom4te.in >autom4te.tmp
	chmod +x autom4te.tmp
	mv autom4te.tmp autom4te

autoreconf: $(srcdir)/autoreconf.in Makefile
	rm -f autoreconf autoreconf.tmp
	$(edit) $(srcdir)/autoreconf.in >autoreconf.tmp
	chmod +x autoreconf.tmp
	mv autoreconf.tmp autoreconf

autoscan: $(srcdir)/autoscan.in Makefile
	rm -f autoscan autoscan.tmp
	$(edit) $(srcdir)/autoscan.in >autoscan.tmp
	chmod +x autoscan.tmp
	mv autoscan.tmp autoscan

autoupdate: $(srcdir)/autoupdate.in Makefile
	rm -f autoupdate autoupdate.tmp
	$(edit) $(srcdir)/autoupdate.in >autoupdate.tmp
	chmod +x autoupdate.tmp
	mv autoupdate.tmp autoupdate

ifnames: $(srcdir)/ifnames.in Makefile
	rm -f ifnames ifnames.tmp
	$(edit) $(srcdir)/ifnames.in >ifnames.tmp
	chmod +x ifnames.tmp
	mv ifnames.tmp ifnames



## --------------- ##
## Building TAGS.  ##
## --------------- ##

TAGS_DEPENDENCIES = $(EXTRA_DIST)

letters = abcdefghijklmnopqrstuvwxyz
LETTERS = ABCDEFGHIJKLMNOPQRSTUVWXYZ
DIGITS = 0123456789
WORD_REGEXP = [$(LETTERS)$(letters)_][$(LETTERS)$(letters)$(DIGITS)_]*
ETAGS_PERL = --lang=perl \
  autoheader.in autoreconf.in autoupdate.in autoscan.in autom4te.in \
  ifnames.in
ETAGS_SH = --lang=none --regex='/\($(WORD_REGEXP)\)=/\1/' \
  autoconf.in

ETAGS_ARGS = $(ETAGS_PERL) $(ETAGS_SH)
