# Makefile for GNU tar documentation.
# Copyright (C) 1994, 1995, 1996, 1997 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

AUTOMAKE_OPTIONS = gnits
info_TEXINFOS = tar.texi

EXTRA_DIST = convtexi.pl getdate.texi header.texi

CLEANFILES = tmp-*

# The rendering level is anyone of PUBLISH, DISTRIB or PROOF.
# Just call `make dvi RENDITION=PROOF' if you want PROOF rendition.
RENDITION = DISTRIB

tar.info: tar.texi getdate.texi header.texi version.texi
	@echo "WARNING: \`makeinfo' will not preprocess Texinfo input properly"
	@echo "         for expanding Texinfo macros, if it comes from a"
	@echo "         Texinfo distribution which is earlier than version 3.7."
	cd $(srcdir) && $(MAKEINFO) -D$(RENDITION) tar.texi

tar.dvi: tar.texi getdate.texi header.texi version.texi
	@echo "WARNING: \`makeinfo' will not preprocess Texinfo input properly"
	@echo "         for expanding Texinfo macros, if it comes from a"
	@echo "         Texinfo distribution which is earlier than version 3.7."
	$(MAKEINFO) -D$(RENDITION) -Etmp-tar.tmp -otmp-tar.info \
	  -I$(srcdir) tar.texi
	rm -f tmp-tar.sed tmp-tar.info*
	test $(RENDITION) = DISTRIB \
	  || echo >>tmp-tar.sed 's/^@set DISTRIB/@set $(RENDITION)/'
	test $(RENDITION) = PUBLISH \
	  || echo >>tmp-tar.sed '/^@smallbook/d'
	sed -f tmp-tar.sed tmp-tar.tmp > tmp-tar.texi
	rm -f tmp-tar.sed tmp-tar.tmp
	TEXINPUTS=$(srcdir):$$TEXINPUTS $(TEXI2DVI) tmp-tar.texi
	mv tmp-tar.dvi $@

header.texi: $(top_srcdir)/src/tar.h
	sed -n '/Archive Format/,/End of Format/p' $(top_srcdir)/src/tar.h \
	  | expand | sed 's/\([{}]\)/@\1/g' > $(srcdir)/header.texi
