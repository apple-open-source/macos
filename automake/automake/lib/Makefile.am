## Process this file with automake to create Makefile.in

## Makefile for Automake lib.

## Copyright 2001 Free Software Foundation, Inc.

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

SUBDIRS = Automake am

dist_pkgvdata_DATA = COPYING INSTALL texinfo.tex ansi2knr.c ansi2knr.1

## These must all be executable when installed.  However, if we use
## _SCRIPTS, then the program transform will be applied, which is not
## what we want.  So we make them executable by hand.
scriptdir = $(pkgvdatadir)
dist_script_DATA = config.guess config.sub install-sh mdate-sh missing \
mkinstalldirs elisp-comp ylwrap acinstall depcomp compile py-compile

install-data-hook:
	@$(POST_INSTALL)
	@for prog in $(dist_script_DATA); do \
	  echo " chmod +x $(DESTDIR)$(scriptdir)/$$prog"; \
	  chmod +x $(DESTDIR)$(scriptdir)/$$prog; \
	done

## `test -x' is not portable.  So we use Perl instead.  If Perl
## doesn't exist, then this test is meaningless anyway.
installcheck-local:
	for file in $(dist_script_DATA); do \
	  $(PERL) -e "exit ! -x '$(pkgvdatadir)/$$file';" || exit 1; \
	done
