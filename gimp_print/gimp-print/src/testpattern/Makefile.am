## $Id: Makefile.am,v 1.1.1.3 2004/12/22 23:49:39 jlovell Exp $
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

pkgdatadir = $(datadir)/$(PACKAGE)/samples

TESTS = run-testpattern run-testpattern-1


## Programs

if BUILD_TESTPATTERN
bin_PROGRAMS = testpattern
noinst_PROGRAMS = printers
endif

AM_LFLAGS = -i
AM_YFLAGS = -d

testpattern_SOURCES = testpattern.c testpatterny.y testpatternl.l testpattern.h
testpattern_LDADD = $(GIMPPRINT_LIBS)

testpatternl.o: testpatterny.o
testpattern.o: testpatterny.o

printers_LDADD = $(GIMPPRINT_LIBS)


## Data

pkgdata_DATA = testpattern.sample extended.sample


## Clean

MAINTAINERCLEANFILES = Makefile.in testpatternl.c testpatterny.c testpatterny.h

EXTRA_DIST =  testpatterny.h $(pkgdata_DATA) run-testpattern run-testpattern-1

