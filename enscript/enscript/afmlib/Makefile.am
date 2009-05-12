#
# Automakefile for AFM library.
# Copyright (c) 1995-1999 Markku Rossi
#
# Author: Markku Rossi <mtr@iki.fi>
#

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
#
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

AUTOMAKE_OPTIONS = ansi2knr

noinst_LIBRARIES = libafm.a
libafm_a_SOURCES = afm.c afmparse.c strhash.c e_88591.c e_88592.c	\
e_88593.c e_88594.c e_88595.c e_88597.c e_88599.c e_885910.c e_pc.c	\
e_mac.c e_vms.c e_hp8.c e_koi8.c deffont.c

noinst_HEADERS = afm.h afmint.h strhash.h

INCLUDES_windelta = +-I../w32/

EXTRA_DIST = ansi2knr.c ansi2knr.1 Makefile-encodings make-encoding.pl

noinst_PROGRAMS = afmtest

afmtest_SOURCES = afmtest.c
afmtest_LDADD = libafm.a
