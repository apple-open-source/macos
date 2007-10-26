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
noinst_HEADERS = system.h localedir.h rmt.h
libtar_a_SOURCES = prepargs.c prepargs.h rtapelib.c

localedir = $(datadir)/locale

DISTCLEANFILES = localedir.h
localedir.h : Makefile
	echo '#define LOCALEDIR "$(localedir)"' >$@
	echo "#ifndef DEFAULT_RMT_COMMAND" >> $@
	echo "# define DEFAULT_RMT_COMMAND \"$(DEFAULT_RMT_DIR)/`echo \"rmt\" | sed 's,^.*/,,;$(transform)'`$(EXEEXT)\"" >> $@
	echo "#endif" >> $@

rtapelib.o: localedir.h

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

SUFFIXES = .o .c .h
CLEANFILES = 
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

# allocsa
libtar_a_SOURCES += allocsa.h allocsa.c
EXTRA_DIST += allocsa.valgrind

# argmatch
libtar_a_SOURCES += argmatch.h argmatch.c

# argp
libtar_a_SOURCES += argp.h argp-ba.c argp-eexst.c \
           argp-fmtstream.c argp-fmtstream.h argp-fs-xinl.c argp-help.c \
           argp-namefrob.h argp-parse.c argp-pv.c argp-pvh.c argp-xinl.c

# backupfile
libtar_a_SOURCES += backupfile.h backupfile.c

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

# getcwd
libtar_a_SOURCES += getcwd.h

# getdate
libtar_a_SOURCES += getdate.h getdate.y
BUILT_SOURCES += getdate.c
MAINTAINERCLEANFILES += getdate.c

# getline
libtar_a_SOURCES += getline.h
EXTRA_DIST += getndelim2.h getndelim2.c

# getopt
BUILT_SOURCES += $(GETOPT_H)
EXTRA_DIST += getopt_.h getopt_int.h

# We need the following in order to create an <getopt.h> when the system
# doesn't have one that works with the given compiler.
all-local $(lib_OBJECTS): $(GETOPT_H)
getopt.h: getopt_.h
	cp $(srcdir)/getopt_.h $@-t
	mv $@-t $@
MOSTLYCLEANFILES += getopt.h getopt.h-t

# getpagesize
libtar_a_SOURCES += getpagesize.h

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
libtar_a_SOURCES += lchown.h stat-macros.h

# localcharset
libtar_a_SOURCES += localcharset.h localcharset.c
EXTRA_DIST += config.charset ref-add.sin ref-del.sin
DEFS += -DLIBDIR=\"$(libdir)\"

# We need the following in order to install a simple file in $(libdir)
# which is shared with other installed packages. We use a list of referencing
# packages so that "make uninstall" will remove the file if and only if it
# is not used by another installed package.
# On systems with glibc-2.1 or newer, the file is redundant, therefore we
# avoid installing it.

all-local: charset.alias ref-add.sed ref-del.sed

charset_alias = $(DESTDIR)$(libdir)/charset.alias
charset_tmp = $(DESTDIR)$(libdir)/charset.tmp
install-exec-local: all-local
	test $(GLIBC21) != no || $(mkinstalldirs) $(DESTDIR)$(libdir)
	if test -f $(charset_alias); then \
	  sed -f ref-add.sed $(charset_alias) > $(charset_tmp) ; \
	  $(INSTALL_DATA) $(charset_tmp) $(charset_alias) ; \
	  rm -f $(charset_tmp) ; \
	else \
	  if test $(GLIBC21) = no; then \
	    sed -f ref-add.sed charset.alias > $(charset_tmp) ; \
	    $(INSTALL_DATA) $(charset_tmp) $(charset_alias) ; \
	    rm -f $(charset_tmp) ; \
	  fi ; \
	fi

uninstall-local: all-local
	if test -f $(charset_alias); then \
	  sed -f ref-del.sed $(charset_alias) > $(charset_tmp); \
	  if grep '^# Packages using this file: $$' $(charset_tmp) \
	      > /dev/null; then \
	    rm -f $(charset_alias); \
	  else \
	    $(INSTALL_DATA) $(charset_tmp) $(charset_alias); \
	  fi; \
	  rm -f $(charset_tmp); \
	fi

charset.alias: config.charset
	$(SHELL) $(srcdir)/config.charset '$(host)' > t-$@
	mv t-$@ $@

SUFFIXES += .sed .sin
.sin.sed:
	sed -e '/^#/d' -e 's/@''PACKAGE''@/$(PACKAGE)/g' $< > t-$@
	mv t-$@ $@

CLEANFILES += charset.alias ref-add.sed ref-del.sed

# mempcpy
libtar_a_SOURCES += mempcpy.h

# memset

# mktime

# modechange
libtar_a_SOURCES += modechange.h modechange.c

# obstack

# quote
libtar_a_SOURCES += quote.h quote.c

# quotearg
libtar_a_SOURCES += quotearg.h quotearg.c

# restrict

# rmdir
libtar_a_SOURCES += stat-macros.h

# safe-read
libtar_a_SOURCES += safe-read.h safe-read.c

# safe-write
libtar_a_SOURCES += safe-write.h safe-write.c

# save-cwd
libtar_a_SOURCES += save-cwd.h save-cwd.c

# savedir
libtar_a_SOURCES += savedir.h savedir.c

# setenv
libtar_a_SOURCES += setenv.h

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

# strchrnul
libtar_a_SOURCES += strchrnul.h

# strndup
libtar_a_SOURCES += strndup.h

# strnlen

# strtoimax

# strtol

# strtoll

# strtoul

# strtoull

# strtoumax

# sysexits
EXTRA_DIST += sysexit_.h

# We need the following in order to create a <sysexits.h> when the system
# doesn't have one that works with the given compiler.
all-local $(lib_OBJECTS): $(SYSEXITS_H)
sysexits.h: sysexit_.h
	cp $(srcdir)/sysexit_.h sysexits.h-t
	mv sysexits.h-t sysexits.h
MOSTLYCLEANFILES += sysexits.h sysexits.h-t

# time_r
libtar_a_SOURCES += time_r.h

# timespec
libtar_a_SOURCES += timespec.h

# unlocked-io
libtar_a_SOURCES += unlocked-io.h

# utime

# xalloc
libtar_a_SOURCES += xalloc.h xmalloc.c

# xalloc-die
libtar_a_SOURCES += xalloc-die.c

# xgetcwd
libtar_a_SOURCES += xgetcwd.h xgetcwd.c

# xstrtol
libtar_a_SOURCES += xstrtol.h xstrtol.c xstrtoul.c

# xstrtoumax
libtar_a_SOURCES += xstrtoumax.c

