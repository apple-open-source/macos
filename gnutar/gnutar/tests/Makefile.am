# Makefile for GNU tar regression tests.

# Copyright (C) 1996, 1997, 1999, 2000, 2001, 2003 Free Software
# Foundation, Inc.

# François Pinard <pinard@iro.umontreal.ca>, 1988.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

check_PROGRAMS = genfile mksparse
BUILT_SOURCES = preset

TESTS = \
 version.sh\
 append.sh\
 delete01.sh\
 delete02.sh\
 delete03.sh\
 delete04.sh\
 extrac01.sh\
 extrac02.sh\
 extrac03.sh\
 extrac04.sh\
 gzip.sh\
 incremen.sh\
 ignfail.sh\
 listed01.sh\
 longv7.sh\
 multiv01.sh\
 multiv02.sh\
 old.sh\
 options.sh\
 same-order01.sh\
 same-order02.sh\
 volume.sh\
 recurse.sh\
 sparse01.sh\
 star/gtarfail.sh\
 star/gtarfail2.sh\
 star/multi-fail.sh\
 star/ustar-big-2g.sh\
 star/ustar-big-8g.sh\
 star/pax-big-10g.sh\
 star/qucktest.sh 

genfile_SOURCES = genfile.c
mksparse_SOURCES = mksparse.c
EXTRA_DIST = after before preset.in $(TESTS) star/README

localedir = $(datadir)/locale
INCLUDES = -I$(top_srcdir)/lib -I../lib -I$(top_srcdir)/src

LDADD = ../lib/libtar.a $(LIBINTL)

mostlyclean:
	rm -rf tmp-*

$(TESTS): genfile mksparse
