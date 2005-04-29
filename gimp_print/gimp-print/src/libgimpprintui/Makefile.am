## $Id: Makefile.am,v 1.1.1.1 2004/07/23 06:26:31 jlovell Exp $
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

m4datadir = $(datadir)/aclocal
pkgconfigdatadir = $(prefix)/lib/pkgconfig


## Programs

if BUILD_LIBGIMPPRINTUI
lib_LTLIBRARIES = libgimpprintui.la
endif

libgimpprintui_la_SOURCES =			\
	printrcy.y				\
	printrcl.l				\
	printrc.h				\
	plist.c					\
	panel.c					\
	ui-utils.c				\
	print-image-thumbnail.c			\
	gimp-print-ui-internal.h

libgimpprintui_la_LIBADD = $(GIMPPRINT_UI_LIBDEPS)
# Uncommment to build an unversioned library (version in soname)
libgimpprintui_la_CFLAGS = $(AM_CFLAGS) $(GIMPPRINT_CFLAGS) $(GTK_CFLAGS)
libgimpprintui_la_LDFLAGS = -release $(GIMPPRINT_VERSION) -rpath $(libdir)
# Uncomment to build a versioned library
#libgimpprintui_la_LDFLAGS = \
# -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
# -rpath $(libdir)

AM_LFLAGS = -i
AM_YFLAGS = -d

printrcy.o: printrcl.o
plist.o: printrcy.o

## Data

pkgconfigdata_DATA = gimpprintui.pc


## Rules


## Clean

MAINTAINERCLEANFILES = Makefile.in printrcl.c printrcy.c printrcy.h
