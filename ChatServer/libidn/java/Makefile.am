## Process this file with automake to produce Makefile.in
# Copyright (C) 2004, 2005, 2006, 2007 Simon Josefsson.
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

SUBDIRS = gnu . misc

INSTALL_TARGETS =

if JAVA
libidn_jardir = $(datadir)/java
dist_libidn_jar_DATA = libidn-@VERSION@.jar

libidn-@VERSION@.jar:
	$(JAR) cf $@ gnu/inet/encoding/CombiningClass.class	\
		gnu/inet/encoding/Composition.class		\
		gnu/inet/encoding/DecompositionKeys.class	\
		gnu/inet/encoding/DecompositionMappings.class	\
		gnu/inet/encoding/IDNA.class			\
		gnu/inet/encoding/IDNAException.class		\
		gnu/inet/encoding/NFKC.class			\
		gnu/inet/encoding/Punycode.class		\
		gnu/inet/encoding/PunycodeException.class	\
		gnu/inet/encoding/RFC3454.class			\
		gnu/inet/encoding/Stringprep.class		\
		gnu/inet/encoding/StringprepException.class

INSTALL_TARGETS += install-jar
install-jar:
	install -d -m 755 $(DESTDIR)$(libidn_jardir)
	ln -sf libidn-@VERSION@.jar $(DESTDIR)$(libidn_jardir)/libidn.jar

clean-local:
	rm -f $(libidn_jar_DATA)

# Invoked by ../doc/java/Makefile.
.PHONY: javadoc
javadoc:
	cd $(srcdir) && $(GJDOC) -d $(top_builddir)/doc/java gnu/inet/encoding/*.java
endif

install-data-local: $(INSTALL_TARGETS)
