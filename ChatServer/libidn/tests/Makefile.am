## Process this file with automake to produce Makefile.in
# Copyright (C) 2002, 2003, 2004, 2006, 2007 Simon Josefsson.
#
# This file is part of GNU Libidn.
#
# GNU Libidn is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# GNU Libidn is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GNU Libidn; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib -I$(top_srcdir)/gl -I../gl
AM_LDFLAGS = -no-install
LDADD = ../lib/libidn.la libutils.la

noinst_LTLIBRARIES = libutils.la
libutils_la_SOURCES = utils.h utils.c

ctests = tst_stringprep$(EXEEXT) tst_punycode$(EXEEXT)		\
	tst_idna$(EXEEXT) tst_idna2$(EXEEXT) tst_nfkc$(EXEEXT)	\
	tst_pr29$(EXEEXT) tst_strerror$(EXEEXT)
if TLD
ctests += tst_tld$(EXEEXT)
endif

TESTS = $(ctests)	
check_PROGRAMS = $(ctests)

TESTS_ENVIRONMENT = $(VALGRIND)
