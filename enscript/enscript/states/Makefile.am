#
# Makefile for states.
# Copyright (c) 1997-1998 Markku Rossi
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

SUBDIRS = tests

AUTOMAKE_OPTIONS =

man_MANS = states.1

bin_PROGRAMS = states
states_SOURCES = main.c utils.c prims.c process.c gram.y lex.l

LDADD = ../afmlib/libafm.a @INTLLIBS@ ../compat/libcompat.a @LEXLIB@
states_DEPENDENCIES = @INTLLIBS@ ../compat/libcompat.a

noinst_HEADERS = defs.h gram.h

EXTRA_DIST = states.1.in TODO enscript.st over.in enscript.el

localedir = $(prefix)/@DATADIRNAME@/locale

bin_SCRIPTS = over

over: $(srcdir)/over.in
	sed 's%@LIBRARYDIR@%$(prefix)/@DATADIRNAME@/enscript%g' \
	$(srcdir)/over.in > over

YFLAGS = -d

DEFS = @DEFS@ -DLOCALEDIR=\"$(localedir)\" -I. -I$(srcdir) -I..
INCLUDES = -I$(srcdir)/../compat -I$(srcdir)/../afmlib -I$(srcdir)/../intl -I../intl

install-data-local: enscript.st
	$(top_srcdir)/mkinstalldirs $(datadir)/enscript
	$(INSTALL_DATA) $(srcdir)/enscript.st $(datadir)/enscript/enscript.st

uninstall-local:
	rm -f $(datadir)/enscript/enscript.st
