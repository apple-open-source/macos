# Main Makefile for GNU tar.
# Copyright 1994, 1995, 1996, 1997, 1999 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

AUTOMAKE_OPTIONS = gnits dist-shar

BABYL = rmail/* admin/*/RMAIL
EXTRA_DIST = PORTS rebox.el
ACLOCAL_AMFLAGS =
SUBDIRS = doc lib intl m4 src scripts po tests

all-local: $(CONFIG_HEADER)

id: ID

ID:
	cd lib && $(MAKE) $@
	cd src && $(MAKE) $@

# This is a bit of a hack; there must be a better way.
M4DIR = $(srcdir)/m4
ACINCLUDE_INPUTS = $(M4DIR)/ccstdc.m4 $(M4DIR)/d-ino.m4 $(M4DIR)/gettext.m4 \
  $(M4DIR)/inttypes_h.m4 $(M4DIR)/isc-posix.m4 $(M4DIR)/jm-mktime.m4 \
  $(M4DIR)/largefile.m4 $(M4DIR)/lcmessage.m4 $(M4DIR)/malloc.m4 \
  $(M4DIR)/mktime.m4 $(M4DIR)/progtest.m4 $(M4DIR)/realloc.m4 \
  $(M4DIR)/uintmax_t.m4 $(M4DIR)/ulonglong.m4 $(M4DIR)/utimbuf.m4 \
  $(M4DIR)/utime.m4 $(M4DIR)/utimes.m4 $(M4DIR)/xstrtoumax.m4

$(srcdir)/acinclude.m4 : $(ACINCLUDE_INPUTS)
	cat $(ACINCLUDE_INPUTS) >$(srcdir)/acinclude.m4

dist-zoo: $(DISTFILES)
	rm -rf $(distdir)
	mkdir $(distdir)
	distdir=`cd $(distdir) && pwd` \
	  && cd $(srcdir) \
	  && automake --include-deps --output-dir=$$distdir
	@for file in $(DISTFILES); do		\
	  test -f $(distdir)/$$file \
	    || cp -p $(srcdir)/$$file $(distdir)/$$file; \
	done
	for subdir in $(SUBDIRS); do		\
	  test -d $(distdir)/$$subdir		\
	  || mkdir $(distdir)/$$subdir		\
	  || exit 1;				\
	  chmod 777 $(distdir)/$$subdir;	\
	  (cd $$subdir && $(MAKE) dist) || exit 1; \
	done
	@sublist="$(DIST_SUBDIRS)";		\
	for dir in $$sublist; do		\
	  echo copying directory $$dir;		\
	  tar -chf - $$dir | (cd $(distdir) && tar -xBpf -); \
	done
	chmod -R a+r $(distdir)
	find $(distdir) -type f | xargs dosfn
#	find $(distdir) -type f | xargs recode :ibmpc
	mv $(distdir) $(PACKAGE)
	find $(PACKAGE) -type f | zoo ahIq $(PACKAGE).zoo
	rm -rf $(PACKAGE)
