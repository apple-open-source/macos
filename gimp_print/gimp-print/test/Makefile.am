## $Id: Makefile.am,v 1.1.1.3 2004/12/22 23:49:40 jlovell Exp $
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

LOCAL_CPPFLAGS = -I$(top_srcdir)/src/main $(GIMPPRINT_CFLAGS)

TESTS = curve run-testdither run-weavetest


## Programs

if BUILD_TEST
noinst_PROGRAMS = testdither escp2-weavetest unprint pcl-unprint bjc-unprint curve xml-curve
endif

escp2_weavetest_SOURCES = escp2-weavetest.c
escp2_weavetest_LDADD = $(GIMPPRINT_LIBS)

unprint_SOURCES = unprint.c
unprint_LDADD = $(GIMPPRINT_LIBS)

curve_SOURCES = curve.c
curve_LDADD = $(GIMPPRINT_LIBS)

pcl_unprint_SOURCES = pcl-unprint.c
pcl_unprint_LDADD = $(GIMPPRINT_LIBS)

bjc_unprint_SOURCES = bjc-unprint.c
bjc_unprint_LDADD = $(GIMPPRINT_LIBS)

testdither_SOURCES = testdither.c
testdither_LDADD = $(GIMPPRINT_LIBS)

xml_curve_SOURCES = xml-curve.c
xml_curve_LDADD = $(GIMPPRINT_LIBS)

## Rules

#run-weavetest: escp2-weavetest


## Clean

CLEANFILES = mixed-color-1bit.ppm
MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = cyan-sweep.tif parse-escp2 run-weavetest run-testdither
