## $id: makefile.am,v 1.9 2001/06/03 18:35:03 rleigh Exp $
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
INCLUDES = @INCLUDES@
LIBPRINTUT = $(top_builddir)/lib/libprintut.la
LIBS = $(INTLLIBS) @LIBS@


## Programs

ESCPUTIL_BIN = escputil

bin_PROGRAMS = @ESCPUTIL_BIN@

EXTRA_PROGRAMS = escputil

escputil_LDADD = $(LIBPRINTUT) $(LIBREADLINE_DEPS)


## Clean

CLEANFILES = $(EXTRA_PROGRAMS)
MAINTAINERCLEANFILES = Makefile.in
