# Makefile for GNU tar library.

# Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2003, 2004
# Free Software Foundation, Inc.

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

noinst_LIBRARIES = libtar.a

libtar_a_SOURCES = prepargs.c prepargs.h

libtar_a_LIBADD = $(LIBOBJS) $(ALLOCA)
libtar_a_DEPENDENCIES = $(libtar_a_LIBADD)

BUILT_SOURCES =
EXTRA_DIST = Makefile.tmpl
MAINTAINERCLEANFILES =
MOSTLYCLEANFILES =
lib_OBJECTS = $(libtar_a_OBJECTS)

# Special rule for getdate
#
# Say $(srcdir), so GNU make does not report an ambiguity with the .y.c rule.
$(srcdir)/getdate.c: getdate.y
	cd $(srcdir) && \
	  $(YACC) $(YFLAGS) getdate.y && \
	  mv -f y.tab.c getdate.c

# gnulib modules

# alloca
BUILT_SOURCES += $(ALLOCA_H)
EXTRA_DIST += alloca_.h

# We need the following in order to create an <alloca.h> when the system
# doesn't have one that works with the given compiler.
all-local $(lib_OBJECTS): $(ALLOCA_H)
alloca.h: alloca_.h
	cp $(srcdir)/alloca_.h $@-t
	mv $@-t $@
MOSTLYCLEANFILES += alloca.h alloca.h-t

# argmatch
libtar_a_SOURCES += argmatch.h argmatch.c

# backupfile
libtar_a_SOURCES += backupfile.h backupfile.c addext.c

# chown

# dirname
libtar_a_SOURCES += dirname.h dirname.c basename.c stripslash.c

# error

# exclude
libtar_a_SOURCES += exclude.h exclude.c

# exit
libtar_a_SOURCES += exit.h

# exitfail
libtar_a_SOURCES += exitfail.h exitfail.c

# extensions

# fileblocks

# fnmatch
BUILT_SOURCES += $(FNMATCH_H)
EXTRA_DIST += fnmatch_.h fnmatch_loop.c

# We need the following in order to create an <fnmatch.h> when the system
# doesn't have one that supports the required API.
all-local $(lib_OBJECTS): $(FNMATCH_H)
fnmatch.h: fnmatch_.h
	cp $(srcdir)/fnmatch_.h $@-t
	mv $@-t $@
MOSTLYCLEANFILES += fnmatch.h fnmatch.h-t

# fnmatch-gnu

# ftruncate

# full-write
libtar_a_SOURCES += full-write.h full-write.c

# getdate
libtar_a_SOURCES += getdate.h getdate.y
BUILT_SOURCES += getdate.c
MAINTAINERCLEANFILES += getdate.c

# getline
libtar_a_SOURCES += getline.h
EXTRA_DIST += getndelim2.h getndelim2.c

# getopt
libtar_a_SOURCES += getopt.h getopt.c getopt1.c getopt_int.h

# gettext
libtar_a_SOURCES += gettext.h

# gettime
libtar_a_SOURCES += gettime.c

# gettimeofday

# hash
libtar_a_SOURCES += hash.h hash.c

# human
libtar_a_SOURCES += human.h human.c

# lchown
libtar_a_SOURCES += lchown.h

# malloc

# memset

# mktime

# modechange
libtar_a_SOURCES += modechange.h modechange.c

# obstack

# pathmax
libtar_a_SOURCES += pathmax.h

# quote
libtar_a_SOURCES += quote.h quote.c

# quotearg
libtar_a_SOURCES += quotearg.h quotearg.c

# realloc

# restrict

# rmdir

# safe-read
libtar_a_SOURCES += safe-read.h safe-read.c

# safe-write
libtar_a_SOURCES += safe-write.h safe-write.c

# save-cwd
libtar_a_SOURCES += save-cwd.h save-cwd.c

# savedir
libtar_a_SOURCES += savedir.h savedir.c

# stdbool
BUILT_SOURCES += $(STDBOOL_H)
EXTRA_DIST += stdbool_.h

# We need the following in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(lib_OBJECTS): $(STDBOOL_H)
stdbool.h: stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/$(HAVE__BOOL)/g' < $(srcdir)/stdbool_.h > $@-t
	mv $@-t $@
MOSTLYCLEANFILES += stdbool.h stdbool.h-t

# stpcpy
libtar_a_SOURCES += stpcpy.h

# strcase
libtar_a_SOURCES += strcase.h

# strtoimax

# strtol

# strtoll

# strtoul

# strtoull

# strtoumax

# time_r
libtar_a_SOURCES += time_r.h

# timespec
libtar_a_SOURCES += timespec.h

# unlocked-io
libtar_a_SOURCES += unlocked-io.h

# utime

# xalloc
libtar_a_SOURCES += xalloc.h xmalloc.c xstrdup.c

# xgetcwd
libtar_a_SOURCES += xgetcwd.h xgetcwd.c

# xstrtol
libtar_a_SOURCES += xstrtol.h xstrtol.c xstrtoul.c

# xstrtoumax
libtar_a_SOURCES += xstrtoumax.c

