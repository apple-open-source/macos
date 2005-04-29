## Makefile for the gettext-runtime/lib subdirectory of GNU gettext
## Copyright (C) 1995-1998, 2000-2003 Free Software Foundation, Inc.
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

AUTOMAKE_OPTIONS = 1.5 gnits no-dependencies
EXTRA_DIST =
MOSTLYCLEANFILES =

noinst_LIBRARIES = libgrt.a

# Sources that are compiled on all platforms.

libgrt_a_SOURCES = \
  ../../gettext-tools/lib/basename.h ../../gettext-tools/lib/basename.c \
  ../../gettext-tools/lib/closeout.h ../../gettext-tools/lib/closeout.c \
  ../../gettext-tools/lib/error.h ../../gettext-tools/lib/error.c \
  ../../gettext-tools/lib/exit.h \
  ../../gettext-tools/lib/fwriteerror.h ../../gettext-tools/lib/fwriteerror.c \
  ../../gettext-tools/lib/getopt.h ../../gettext-tools/lib/getopt.c ../../gettext-tools/lib/getopt1.c \
  ../../gettext-tools/lib/pathmax.h \
  ../../gettext-tools/lib/progname.h ../../gettext-tools/lib/progname.c ../../gettext-tools/lib/progreloc.c \
  ../../gettext-tools/lib/strtoul.c \
  ../../gettext-tools/lib/unlocked-io.h \
  ../../gettext-tools/lib/xalloc.h ../../gettext-tools/lib/xmalloc.c ../../gettext-tools/lib/xstrdup.c \
  ../../gettext-tools/lib/xreadlink.h ../../gettext-tools/lib/xreadlink.c

# Sources that are compiled only on platforms that lack the functions.

LIBADD_SOURCE = \
  alloca.c \
  atexit.c \
  ../../gettext-tools/lib/canonicalize.h canonicalize.c \
  memmove.c \
  readlink.c \
  ../../gettext-tools/lib/relocatable.h relocatable.c \
  ../../gettext-tools/lib/setenv.h setenv.c unsetenv.c \
  strerror.c

# How to build libgrt.a.

libgrt_a_LIBADD = @ALLOCA@ @LIBOBJS@

AM_CPPFLAGS = \
  -I. -I$(srcdir) \
  -I.. \
  -I../intl -I$(srcdir)/../intl \
  -I../../gettext-tools/lib -I$(srcdir)/../../gettext-tools/lib

DEFS = -DDEPENDS_ON_LIBINTL=1 @DEFS@

# List of files to be distributed.

EXTRA_DIST += $(LIBADD_SOURCE)


# >>> gnulib module stdbool.
EXTRA_DIST += ../../gettext-tools/lib/stdbool_.h

# The following is needed in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(libgrt_a_OBJECTS): @STDBOOL_H@
stdbool.h: ../../gettext-tools/lib/stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/@HAVE__BOOL@/g' < $(srcdir)/../../gettext-tools/lib/stdbool_.h > stdbool.h
MOSTLYCLEANFILES += stdbool.h
# <<< gnulib module stdbool.


# >>> gnulib module alloca.
EXTRA_DIST += ../../gettext-tools/lib/alloca_.h

# The following is needed in order to create an <alloca.h> when the system
# doesn't have one that works with the given compiler.
all-local $(libgrt_a_OBJECTS): @ALLOCA_H@
alloca.h: ../../gettext-tools/lib/alloca_.h
	cp $(srcdir)/../../gettext-tools/lib/alloca_.h alloca.h
MOSTLYCLEANFILES += alloca.h
# <<< gnulib module alloca.


# VMS support.

EXTRA_DIST += Makefile.vms


# Woe32 support.

EXTRA_DIST += Makefile.msvc
