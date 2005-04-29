## $Id: Makefile.am,v 1.1.1.4 2004/12/22 23:49:37 jlovell Exp $
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

include $(top_srcdir)/scripts/global.mk


## Variables

pkgconfigdatadir = $(prefix)/lib/pkgconfig
pkgxmldatadir = $(pkgdatadir)/$(VERSION)/xml
pkgmoduledir = $(pkglibdir)/$(VERSION)/modules


## Programs

if BUILD_MODULES
pkgmodule_LTLIBRARIES = \
	color-traditional.la \
	print-canon.la \
	print-escp2.la \
	print-lexmark.la \
	print-pcl.la \
	print-ps.la \
	print-olympus.la \
	print-raw.la
endif

color_traditional_la_SOURCES = \
	print-color.c \
	color-conversion.h \
	color-conversions.c

color_traditional_la_LDFLAGS = -module -avoid-version

print_canon_la_SOURCES = print-canon.c
print_canon_la_LDFLAGS = -module -avoid-version

print_escp2_la_SOURCES = \
	print-escp2.c \
	escp2-driver.c \
	print-escp2.h \
	print-escp2-data.c \
	escp2-channels.c \
	escp2-inks.c \
	escp2-papers.c \
	escp2-resolutions.c
print_escp2_la_LDFLAGS = -module -avoid-version

print_lexmark_la_SOURCES = print-lexmark.c
print_lexmark_la_LDFLAGS = -module -avoid-version

print_pcl_la_SOURCES = print-pcl.c
print_pcl_la_LDFLAGS = -module -avoid-version

print_ps_la_SOURCES = print-ps.c
print_ps_la_LDFLAGS = -module -avoid-version

print_olympus_la_SOURCES = print-olympus.c
print_olympus_la_LDFLAGS = -module -avoid-version

print_raw_la_SOURCES = print-raw.c
print_raw_la_LDFLAGS = -module -avoid-version

lib_LTLIBRARIES = libgimpprint.la

module_SOURCES =				\
	$(color_traditional_la_SOURCES)		\
	$(print_canon_la_SOURCES)		\
	$(print_escp2_la_SOURCES)		\
	$(print_lexmark_la_SOURCES)		\
	$(print_pcl_la_SOURCES)			\
	$(print_ps_la_SOURCES)			\
	$(print_olympus_la_SOURCES)		\
	$(print_raw_la_SOURCES)

if !BUILD_MODULES
libgimpprint_modules = $(module_SOURCES)
endif

mxml_SOURCES =					\
	mxml-attr.c				\
	mxml-file.c				\
	mxml-node.c				\
	mxml-search.c

libgimpprint_headers =				\
	dither-impl.h				\
	dither-inlined-functions.h		\
	generic-options.h			\
	gimp-print-internal.h			\
	util.h

libgimpprint_la_SOURCES =			\
	array.c					\
	bit-ops.c				\
	channel.c				\
	color.c					\
	curve.c					\
	curve-cache.c				\
	dither-ed.c				\
	dither-eventone.c			\
	dither-inks.c				\
	dither-main.c				\
	dither-ordered.c			\
	dither-very-fast.c			\
	dither-predithered.c			\
	generic-options.c			\
	image.c					\
	module.c				\
	path.c					\
	print-dither-matrices.c			\
	print-list.c				\
	print-papers.c				\
	print-util.c				\
	print-vars.c				\
	print-version.c				\
	print-weave.c				\
	printers.c				\
	sequence.c				\
	string-list.c				\
	xml.c					\
	$(mxml_SOURCES)				\
	$(libgimpprint_headers)			\
	$(libgimpprint_modules)

libgimpprint_la_LIBADD = $(GIMPPRINT_LIBDEPS)
# Uncommment to build an unversioned library (version in soname)
libgimpprint_version = -release $(GIMPPRINT_VERSION)
# Uncomment to build a versioned library
#libgimpprint_version = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libgimpprint_la_LDFLAGS = $(libgimpprint_version) \
	-rpath $(libdir)
# libtool currently needs a patch for symbol export to work correctly
# using a linker version script.
#	-export-symbols $(srcdir)/libgimpprint.sym


## Data

pkgconfigdata_DATA = gimpprint.pc

pkgxmldata_DATA = \
	dither-matrix-1x1.xml \
	dither-matrix-2x1.xml \
	dither-matrix-4x1.xml \
	papers.xml \
	printers.xml



## Rules

all-local: xmli18n-tmp.h


xmli18n-tmp.h:  $(srcdir)/papers.xml $(srcdir)/printers.xml
# Pull out translatable strings from XML files.  This is crude, but
# works for now.
	sed -e '/description.*value/!d' \
	  -e 's/.*description.*value="\([^"]*\)".*/N_("\1");/' \
	  < $(srcdir)/papers.xml > $@;
	sed -e '/printer.*name/!d' \
	  -e 's/.*printer.*name="\([^"]*\)".*/N_("\1");/' \
	  < $(srcdir)/printers.xml >> $@;

dist-hook: xmli18n-tmp.h
# xmli18n-tmp.h is needed by po/POTFILES.in at dist time


## Clean

CLEANFILES = xmli18n-tmp.h
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = $(pkgxmldata_DATA) libgimpprint.sym
