#
# Makefile for enscript.
# Copyright (c) 1995 Markku Rossi
#
# Author: Markku Rossi <mtr@iki.fi>
#

#
# This file is part of GNU enscript.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

AUTOMAKE_OPTIONS = ansi2knr

bin_PROGRAMS = enscript mkafmmap
enscript_SOURCES = main.c psgen.c util.c prt_lpr.c
mkafmmap_SOURCES = mkafmmap.c

LDADD = ../afmlib/libafm.a @INTLLIBS@ ../compat/libcompat.a -lm
enscript_DEPENDENCIES = ../afmlib/libafm.a @INTLLIBS@ ../compat/libcompat.a
mkafmmap_DEPENDENCIES = ../afmlib/libafm.a @INTLLIBS@ ../compat/libcompat.a

noinst_HEADERS = gsint.h

localedir = $(prefix)/@DATADIRNAME@/locale

DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\" -DLIBRARY=\"$(datadir)/enscript\" -DSYSCONFDIR=\"$(sysconfdir)\" -I. -I$(srcdir) -I..
INCLUDES = -I$(srcdir)/../compat -I$(srcdir)/../afmlib -I$(srcdir)/../intl -I../intl

EXTRA_DIST = prt_dos.c ansi2knr.c ansi2knr.1
