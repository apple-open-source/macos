# Make gzip (GNU zip).

## Copyright (C) 1999, 2001, 2002 Free Software Foundation

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

AUTOMAKE_OPTIONS = gnits dist-shar
ACLOCAL_AMFLAGS = -I m4

man_MANS = gunzip.1 gzexe.1 gzip.1 \
  zcat.1 zcmp.1 zdiff.1 zforce.1 zgrep.1 zless.1 zmore.1 znew.1

M4DIR = $(srcdir)/m4
ACINCLUDE_INPUTS = $(M4DIR)/shell.m4

EXTRA_DIST = $(ACINCLUDE_INPUTS) $(man_MANS) \
  algorithm.doc amiga/match.a amiga/Makefile.gcc amiga/Makefile.sasc \
  amiga/tailor.c amiga/utime.h atari/Makefile.st crypt.h getopt.h \
  gzexe.in gzip.doc gzip.h lzw.h msdos/match.asm msdos/tailor.c \
  msdos/gzip.prj msdos/doturboc.bat msdos/Makefile.msc msdos/Makefile.bor \
  msdos/Makefile.djg nt/Makefile.nt os2/gzip.def os2/Makefile.os2 \
  os2/gzip16.def primos/include/errno.h primos/include/fcntl.h \
  primos/include/stdlib.h primos/include/sysStat.h \
  primos/include/sysTypes.h primos/build.cpl primos/readme \
  primos/primos.c primos/ci.opts revision.h sample/makecrc.c \
  sample/ztouch sample/add.c sample/sub.c sample/zread.c sample/zfile \
  tailor.h vms/Makefile.mms vms/gzip.hlp vms/vms.c vms/Readme.vms \
  vms/Makefile.vms vms/Makefile.gcc vms/makegzip.com zdiff.in \
  zforce.in zgrep.in zless.in zmore.in znew.in

bin_PROGRAMS = gzip
bin_SCRIPTS = gzexe zdiff zgrep zforce zless zmore znew
gzip_SOURCES = \
  bits.c crypt.c deflate.c getopt.c getopt1.c gzip.c inflate.c lzw.c \
  trees.c unlzh.c unlzw.c unpack.c unzip.c util.c yesno.c zip.c
gzip_LDADD = @LIBOBJS@

AM_MAKEINFOFLAGS = --no-split

info_TEXINFOS = gzip.texi
gzip_TEXINFOS = fdl.texi

ASCPP = @ASCPP@
LN_S = @LN_S@

match.$(OBJEXT): match.c
	cp $(srcdir)/match.c _match.S
	$(ASCPP) _match.S >_match.s
	$(CC) -c $(CFLAGS) _match.s
	mv _match.$(OBJEXT) match.$(OBJEXT)
	rm -f _match.S _match.s

gzip.doc: gzip.1
	groff -man -Tascii $(srcdir)/gzip.1 | col -b | uniq >$@

SUFFIXES = .in
.in:
	sed \
		-e '$(SEDCMD)' \
		-e 's|/bin/sh|$(SHELL)|g' \
		-e 's|BINDIR|$(bindir)|g' \
		$(srcdir)/$@.in >$@
	chmod a+x $@

install-exec-hook: remove-installed-links
install-exec-hook remove-installed-links:
	@for prog_ext in $(bin_PROGRAMS) $(bin_SCRIPTS); do \
	  prog=`echo "$$prog_ext"|sed 's/$(EXEEXT)$$//'`; \
	  case $$prog in \
	  gzip) aliases='gunzip zcat';; \
	  zdiff) aliases='zcmp';; \
	  zgrep) aliases='zegrep zfgrep';; \
	  *) continue;; \
	  esac; \
	  transform='$(transform)'; \
	  test "X$$prog" = "X$$prog_ext" || \
	    transform="$$transform"';s/$$/$(EXEEXT)/'; \
	  source=$(DESTDIR)$(bindir)/`echo "$$prog"|sed "$$transform"`; \
	  for alias in $$aliases; do \
	    dest=$(DESTDIR)$(bindir)/`echo "$$alias"|sed "$$transform"`; \
	    (set -x; \
	     rm -f "$$dest" && \
	     case $@ in \
	     install-exec-hook) \
	       for ln in ln "$(LN_S)"; do \
		 $$ln "$$source" "$$dest" && break; \
	       done;; \
	     esac \
	    ) || exit; \
	  done; \
	done

uninstall-local: remove-installed-links

MAINTAINERCLEANFILES = gzip.doc

MOSTLYCLEANFILES = _match.s gzexe zdiff zforce zgrep zless zmore znew
