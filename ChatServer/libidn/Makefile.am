## Process this file with automake to produce Makefile.in
# Copyright (C) 2002, 2003, 2004, 2005 Simon Josefsson.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

SUBDIRS = gl lib po src doc tests examples java csharp

ACLOCAL_AMFLAGS = -I m4 -I gl/m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

EXTRA_DIST = FAQ libc/README libc/getaddrinfo-idn.txt libc/example.c \
	contrib/README contrib/idn-python/README contrib/idn-python/Makefile \
	contrib/idn-python/idn.c contrib/idn-python/test.py \
	contrib/web/idn.php \
	contrib/java/README contrib/java/autogen.sh \
	contrib/java/configure.ac contrib/java/Makefile.am \
	contrib/java/ExampleIDNA.java contrib/java/IDNA.java \
	contrib/java/IDNA.c \
	contrib/doxygen/Doxyfile.in contrib/doxygen/Doxyfile.orig \
	contrib/doxygen/gdoc2doxygen

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libidn.pc

# Maintainer targets

POURL = ftp://ftp.unex.es/pub/gnu-i18n/po/maint/$(PACKAGE)
update-po:
	cd po && \
	wget --no-verbose --timestamping --retr-symlinks $(POURL)/*.po && \
	cvs add `ls *.po | grep -v quot.po` || true && \
	ls *.po | sed 's/.po//' > LINGUAS && \
	cvs commit -m "Sync with TP." LINGUAS `ls *.po | grep -v quot.po`

ChangeLog:
	cvs2cl --FSF --fsf --usermap .cvsusers -I ChangeLog -I .cvs \
		-I autogen.sh -I contrib -I doc/specifications/ -I draft- && \
	cat .cvscopying >> ChangeLog

indent:
	(cd lib && make indent)
	(cd examples && make indent)
	(cd src && make indent)
	(cd tests && make indent)

tag = $(PACKAGE)-`echo $(VERSION) | sed 's/\./-/g'`
htmldir = ../www-$(PACKAGE)

release:
	cvs upd -p -r HEAD NEWS > /dev/null
	if cvs upd -p -r $(tag) NEWS > /dev/null; then false else true; fi
	rm -f ChangeLog && cvs upd ChangeLog
	cvs commit
	rm -f ChangeLog
	$(MAKE) ChangeLog distcheck
	cvs commit -m "" ChangeLog
	cvs rtag $(tag) $(PACKAGE)
	gnupload --to alpha.gnu.org:libidn $(distdir).tar.gz
	cd doc && ./gendocs.sh --html "--css-include=texinfo.css" \
		-o ../$(htmldir)/manual/ $(PACKAGE) "GNU Libidn"
	cp -v $(distdir).tar.gz{,.sig} $(htmldir)/releases/
	cp -v doc/reference/html/*.{html,png,devhelp,css} $(htmldir)/reference/
	cd contrib/doxygen && doxygen && cd ../.. && cp -v contrib/doxygen/html/* $(htmldir)/doxygen/ && cd contrib/doxygen/latex && make refman.pdf && cd ../../../ && cp contrib/doxygen/latex/refman.pdf $(htmldir)/doxygen/$(PACKAGE).pdf
	cp -v doc/java/*.html $(htmldir)/javadoc/
	cd $(htmldir) && cvs add -kb releases/$(distdir).tar.gz{,.sig} && \
		cvs commit -m "Update." manual/ javadoc/ reference/ \
					doxygen/ releases/
