## Process this file with automake to produce Makefile.in
# Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007 Simon Josefsson.
# Copyright (C) 2004 Free Software Foundation, Inc.
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

EXTRA_DIST = gen-stringprep-tables.pl gen-unicode-tables.pl gen-tld-tables.pl

AM_CPPFLAGS = -I$(srcdir)/gl -Igl
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

lib_LTLIBRARIES = libidn.la

idn_int = idn-int.h

BUILT_SOURCES = $(idn_int) gunibreak.h gunicomp.h gunidecomp.h rfc3454.c tlds.c
DISTCLEANFILES = $(idn_int)
include_HEADERS = stringprep.h idna.h punycode.h idn-free.h pr29.h
if TLD
include_HEADERS += tld.h
endif
nodist_include_HEADERS = $(idn_int)

idn-int.h:
	if test -n "$(STDINT_H)"; then \
		cp gl/stdint.h idn-int.h; \
	else \
		echo '#include <stdint.h>' > idn-int.h; \
	fi

libidn_la_SOURCES = gunibreak.h gunicomp.h gunidecomp.h \
	nfkc.c toutf8.c version.c \
	stringprep.h.in stringprep.c rfc3454.c profiles.c \
	punycode.h punycode.c \
	idna.h idna.c \
	pr29.h pr29.c \
	idn-free.h idn-free.c \
	strerror-idna.c strerror-pr29.c strerror-punycode.c \
	strerror-stringprep.c strerror-tld.c

if TLD
libidn_la_SOURCES += tld.h tld.c tlds.c
endif
libidn_la_LIBADD = $(LTLIBICONV) $(LTLIBINTL) gl/libgnu.la
libidn_la_LDFLAGS = \
	-export-symbols-regex '^(idn|pr29_|punycode_|stringprep|tld_).*' \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -no-undefined

nfkc.c: gunibreak.h gunicomp.h gunidecomp.h

SPEC = $(top_srcdir)/doc/specifications

gunibreak.h gunicomp.h gunidecomp.h: gen-unicode-tables.pl $(SPEC)/UnicodeData-3.2.0.txt $(SPEC)/LineBreak-3.2.0.txt $(SPEC)/SpecialCasing-3.2.0.txt $(SPEC)/CaseFolding-3.2.0.txt $(SPEC)/CompositionExclusions-3.2.0.txt
	$(PERL) $(srcdir)/gen-unicode-tables.pl -decomp 3.2 $(SPEC)

rfc3454.c: gen-stringprep-tables.pl
	$(PERL) $(srcdir)/gen-stringprep-tables.pl $(SPEC)/rfc3454.txt

tlds.c: gen-tld-tables.pl
	$(PERL) $(srcdir)/gen-tld-tables.pl $(top_srcdir)/doc/tld/*.tld > tlds.c
