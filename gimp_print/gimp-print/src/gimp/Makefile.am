## $Id: Makefile.am,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $
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


## Variables

AM_CFLAGS = $(GNUCFLAGS)
INCLUDES = @INCLUDES@ $(GIMP_CFLAGS) $(LIBGIMPPRINT_CFLAGS)
LIBS = $(INTLLIBS) @LIBS@
LIBPRINTUT = $(top_builddir)/lib/libprintut.la

libexecdir = $(gimptool_prefix)/plug-ins


## Programs

GIMP_BIN = print

gimp_plug_in_PROGRAMS = @GIMP_BIN@

EXTRA_PROGRAMS = print

print_SOURCES = \
	print-image-gimp.c \
	print-intl.h \
	print.c \
	gimp_color_window.c \
	gimp_main_window.c \
	print_gimp.h
print_LDADD = $(LIBPRINTUT) $(GIMP_LIBS) $(LIBGIMPPRINT_LIBS)
print_DEPENDENCIES = $(LIBGIMPPRINT_LIBS)


## Clean

CLEANFILES = $(EXTRA_PROGRAMS)
MAINTAINERCLEANFILES = Makefile.in
