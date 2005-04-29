## $Id: Makefile.am,v 1.1.1.2 2004/12/22 23:49:35 jlovell Exp $
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

LOCAL_CPPFLAGS = $(GIMPPRINT_CFLAGS) $(GIMP2_CFLAGS)


## Programs

if BUILD_GIMP2
gimp2_plug_in_PROGRAMS = print
endif

print_SOURCES = \
	print-image-gimp.c \
	print-intl.h \
	print.c \
	print_gimp.h

print_LDADD = $(LIBPRINTUT) $(GIMP2_LIBS) $(GIMPPRINTUI2_LIBS)
print_DEPENDENCIES = $(GIMPPRINTUI2_LIBS) $(GIMPPRINT_LIBS)


## Clean

MAINTAINERCLEANFILES = Makefile.in
