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


noinst_LTLIBRARIES = libprintut.la

libprintut_la_SOURCES =

libprintut_la_LIBADD = @LTALLOCA@ @LTLIBOBJS@


noinst_HEADERS = getopt.h libprintut.h pathmax.h xmalloc.h


MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = alloca.c getopt.c getopt1.c strcspn.c vasprintf.c xgetcwd.c xmalloc.c
