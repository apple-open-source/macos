## Makefile for the autoconf-lib-link directory of GNU gettext
## Copyright (C) 1995-1999, 2000-2003 Free Software Foundation, Inc.
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

AUTOMAKE_OPTIONS = 1.5 gnu no-dependencies
ACLOCAL_AMFLAGS = -I m4 -I ../config/m4
EXTRA_DIST =
DISTCLEANFILES =

SUBDIRS = m4 tests

EXTRA_DIST += config.rpath


# We want to maintain only one copy of config.rpath. The one in this directory
# is the source.

all-local: $(srcdir)/../config/config.rpath

$(srcdir)/../config/config.rpath: $(srcdir)/config.rpath
	cp $(srcdir)/config.rpath $(srcdir)/../config/config.rpath


# The AC_CONFIG_AUX_DIR argument in the tests/rpath*/configure.ac files is
# interpreted in two different contexts:
#   1. at "make distrib" time, when we call automake, relative to
#      SRCDIR/tests/rpath*/,
#   2. at "make check" time, when the directory is copied and configured,
#      relative to BUILDDIR/tests/tstdir/rp-*/.
# We choose this argument to be '../../../config', for case 1, and therefore
# we need to make a local copy of the config directory, for case 2.

all-local: config/config.guess config/config.sub config/install-sh config/ltmain.sh config/mkinstalldirs config/missing config/config.rpath

config-dir:
	test -d config || mkdir config
	touch config-dir

config/config.guess: $(srcdir)/../config/config.guess config-dir
	cp $(srcdir)/../config/config.guess config/config.guess

config/config.sub: $(srcdir)/../config/config.sub config-dir
	cp $(srcdir)/../config/config.sub config/config.sub

config/install-sh: $(srcdir)/../config/install-sh config-dir
	cp $(srcdir)/../config/install-sh config/install-sh

config/ltmain.sh: $(srcdir)/../config/ltmain.sh config-dir
	cp $(srcdir)/../config/ltmain.sh config/ltmain.sh

config/mkinstalldirs: $(srcdir)/../config/mkinstalldirs config-dir
	cp $(srcdir)/../config/mkinstalldirs config/mkinstalldirs

config/missing: $(srcdir)/../config/missing config-dir
	cp $(srcdir)/../config/missing config/missing

config/config.rpath: $(srcdir)/config.rpath config-dir
	cp $(srcdir)/config.rpath config/config.rpath

DISTCLEANFILES += config/config.guess config/config.sub config/install-sh config/ltmain.sh config/mkinstalldirs config/missing config/config.rpath

distclean-am:
	rm -rf config config-dir


# Documentation tag not known to automake.

html:


# Subdirectories with separate autoconfiguration.

CONFIGURES_RPATHX = $(srcdir)/tests/rpathx/Makefile.in $(srcdir)/tests/rpathx/aclocal.m4 $(srcdir)/tests/rpathx/configure
$(srcdir)/tests/rpathx/Makefile.in: $(srcdir)/tests/rpathx/Makefile.am $(srcdir)/tests/rpathx/configure.ac $(srcdir)/tests/rpathx/aclocal.m4
	cd $(srcdir)/tests/rpathx && automake Makefile
$(srcdir)/tests/rpathx/aclocal.m4: $(srcdir)/tests/rpathx/configure.ac $(srcdir)/../config/m4/libtool.m4
	cd $(srcdir)/tests/rpathx && aclocal -I ../../m4 -I ../../../config/m4
$(srcdir)/tests/rpathx/configure: $(srcdir)/tests/rpathx/configure.ac $(srcdir)/tests/rpathx/aclocal.m4
	cd $(srcdir)/tests/rpathx && autoconf

CONFIGURES_RPATHY = $(srcdir)/tests/rpathy/Makefile.in $(srcdir)/tests/rpathy/aclocal.m4 $(srcdir)/tests/rpathy/configure
$(srcdir)/tests/rpathy/Makefile.in: $(srcdir)/tests/rpathy/Makefile.am $(srcdir)/tests/rpathy/configure.ac $(srcdir)/tests/rpathy/aclocal.m4
	cd $(srcdir)/tests/rpathy && automake Makefile
$(srcdir)/tests/rpathy/aclocal.m4: $(srcdir)/tests/rpathy/configure.ac $(srcdir)/../config/m4/libtool.m4 $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathy && aclocal -I ../../m4 -I ../../../config/m4
$(srcdir)/tests/rpathy/configure: $(srcdir)/tests/rpathy/configure.ac $(srcdir)/tests/rpathy/aclocal.m4
	cd $(srcdir)/tests/rpathy && autoconf

CONFIGURES_RPATHZ = $(srcdir)/tests/rpathz/Makefile.in $(srcdir)/tests/rpathz/aclocal.m4 $(srcdir)/tests/rpathz/configure
$(srcdir)/tests/rpathz/Makefile.in: $(srcdir)/tests/rpathz/Makefile.am $(srcdir)/tests/rpathz/configure.ac $(srcdir)/tests/rpathz/aclocal.m4
	cd $(srcdir)/tests/rpathz && automake Makefile
$(srcdir)/tests/rpathz/aclocal.m4: $(srcdir)/tests/rpathz/configure.ac $(srcdir)/../config/m4/libtool.m4 $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathz && aclocal -I ../../m4 -I ../../../config/m4
$(srcdir)/tests/rpathz/configure: $(srcdir)/tests/rpathz/configure.ac $(srcdir)/tests/rpathz/aclocal.m4
	cd $(srcdir)/tests/rpathz && autoconf

CONFIGURES_RPATHLX = $(srcdir)/tests/rpathlx/Makefile.in $(srcdir)/tests/rpathlx/aclocal.m4 $(srcdir)/tests/rpathlx/configure
$(srcdir)/tests/rpathlx/Makefile.in: $(srcdir)/tests/rpathlx/Makefile.am $(srcdir)/tests/rpathlx/configure.ac $(srcdir)/tests/rpathlx/aclocal.m4
	cd $(srcdir)/tests/rpathlx && automake Makefile
$(srcdir)/tests/rpathlx/aclocal.m4: $(srcdir)/tests/rpathlx/configure.ac $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathlx && aclocal -I ../../m4
$(srcdir)/tests/rpathlx/configure: $(srcdir)/tests/rpathlx/configure.ac $(srcdir)/tests/rpathlx/aclocal.m4
	cd $(srcdir)/tests/rpathlx && autoconf

CONFIGURES_RPATHLY = $(srcdir)/tests/rpathly/Makefile.in $(srcdir)/tests/rpathly/aclocal.m4 $(srcdir)/tests/rpathly/configure
$(srcdir)/tests/rpathly/Makefile.in: $(srcdir)/tests/rpathly/Makefile.am $(srcdir)/tests/rpathly/configure.ac $(srcdir)/tests/rpathly/aclocal.m4
	cd $(srcdir)/tests/rpathly && automake Makefile
$(srcdir)/tests/rpathly/aclocal.m4: $(srcdir)/tests/rpathly/configure.ac $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathly && aclocal -I ../../m4
$(srcdir)/tests/rpathly/configure: $(srcdir)/tests/rpathly/configure.ac $(srcdir)/tests/rpathly/aclocal.m4
	cd $(srcdir)/tests/rpathly && autoconf

CONFIGURES_RPATHLYX = $(srcdir)/tests/rpathlyx/Makefile.in $(srcdir)/tests/rpathlyx/aclocal.m4 $(srcdir)/tests/rpathlyx/configure
$(srcdir)/tests/rpathlyx/Makefile.in: $(srcdir)/tests/rpathlyx/Makefile.am $(srcdir)/tests/rpathlyx/configure.ac $(srcdir)/tests/rpathlyx/aclocal.m4
	cd $(srcdir)/tests/rpathlyx && automake Makefile
$(srcdir)/tests/rpathlyx/aclocal.m4: $(srcdir)/tests/rpathlyx/configure.ac $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathlyx && aclocal -I ../../m4
$(srcdir)/tests/rpathlyx/configure: $(srcdir)/tests/rpathlyx/configure.ac $(srcdir)/tests/rpathlyx/aclocal.m4
	cd $(srcdir)/tests/rpathlyx && autoconf

CONFIGURES_RPATHLZ = $(srcdir)/tests/rpathlz/Makefile.in $(srcdir)/tests/rpathlz/aclocal.m4 $(srcdir)/tests/rpathlz/configure
$(srcdir)/tests/rpathlz/Makefile.in: $(srcdir)/tests/rpathlz/Makefile.am $(srcdir)/tests/rpathlz/configure.ac $(srcdir)/tests/rpathlz/aclocal.m4
	cd $(srcdir)/tests/rpathlz && automake Makefile
$(srcdir)/tests/rpathlz/aclocal.m4: $(srcdir)/tests/rpathlz/configure.ac $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathlz && aclocal -I ../../m4
$(srcdir)/tests/rpathlz/configure: $(srcdir)/tests/rpathlz/configure.ac $(srcdir)/tests/rpathlz/aclocal.m4
	cd $(srcdir)/tests/rpathlz && autoconf

CONFIGURES_RPATHLZYX = $(srcdir)/tests/rpathlzyx/Makefile.in $(srcdir)/tests/rpathlzyx/aclocal.m4 $(srcdir)/tests/rpathlzyx/configure
$(srcdir)/tests/rpathlzyx/Makefile.in: $(srcdir)/tests/rpathlzyx/Makefile.am $(srcdir)/tests/rpathlzyx/configure.ac $(srcdir)/tests/rpathlzyx/aclocal.m4
	cd $(srcdir)/tests/rpathlzyx && automake Makefile
$(srcdir)/tests/rpathlzyx/aclocal.m4: $(srcdir)/tests/rpathlzyx/configure.ac $(srcdir)/m4/lib-link.m4 $(srcdir)/m4/lib-prefix.m4 $(srcdir)/m4/lib-ld.m4
	cd $(srcdir)/tests/rpathlzyx && aclocal -I ../../m4
$(srcdir)/tests/rpathlzyx/configure: $(srcdir)/tests/rpathlzyx/configure.ac $(srcdir)/tests/rpathlzyx/aclocal.m4
	cd $(srcdir)/tests/rpathlzyx && autoconf

SUBCONFIGURES = $(CONFIGURES_RPATHX) $(CONFIGURES_RPATHY) $(CONFIGURES_RPATHZ) $(CONFIGURES_RPATHLX) $(CONFIGURES_RPATHLY) $(CONFIGURES_RPATHLYX) $(CONFIGURES_RPATHLZ) $(CONFIGURES_RPATHLZYX)

subconfigures: $(SUBCONFIGURES)

# Hidden from automake, but really activated. Works around an automake-1.5 bug.
#distdir: subconfigures
