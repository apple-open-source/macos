# Makefile for GNU tar library.
# Copyright 1994, 1995, 1996, 1997, 1999 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

AUTOMAKE_OPTIONS = gnits ../src/ansi2knr

noinst_LIBRARIES = libtar.a

EXTRA_DIST = \
alloca.c execlp.c fileblocks.c fnmatch.c ftruncate.c lchown.c \
malloc.c memset.c mkdir.c mktime.c realloc.c rename.c rmdir.c \
stpcpy.c strstr.c strtol.c strtoul.c strtoull.c strtoumax.c utime.c

noinst_HEADERS = \
argmatch.h backupfile.h basename.h error.h exclude.h \
fnmatch.h getopt.h getdate.h getpagesize.h \
lchown.h modechange.h pathmax.h quotearg.h safe-read.h xalloc.h xstrtol.h

libtar_a_SOURCES = \
addext.c argmatch.c backupfile.c basename.c error.c \
exclude.c full-write.c getdate.y getopt.c getopt1.c \
modechange.c msleep.c quotearg.c safe-read.c xgetcwd.c xmalloc.c xstrdup.c \
xstrtol.c xstrtoul.c xstrtoumax.c

INCLUDES = -I.. -I$(srcdir) -I../intl

libtar_a_LIBADD = @ALLOCA@ @LIBOBJS@
libtar_a_DEPENDENCIES = $(libtar_a_LIBADD)

getdate.c: getdate.y
	  $(YACC) $(YFLAGS) $<; \
	  mv -f y.tab.c $@
