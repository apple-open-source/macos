## Process this file with automake to produce Makefile.in
# Copyright (C) 2005 Simon Josefsson.
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

CSHARPCOMP = $(SHELL) ../gl/csharpcomp.sh
CSHARPCOMPFLAGS =

SOURCES = AssemblyInfo.cs IDNA.cs RFC3454.cs CombiningClass.cs	\
	IDNAException.cs Stringprep.cs Composition.cs NFKC.cs	\
	StringprepException.cs DecompositionKeys.cs Punycode.cs	\
	DecompositionMappings.cs PunycodeException.cs

MSVSC = libidn.csproj libidn.csproj.user libidn_PPC.csdproj	\
	libidn_PPC.csdproj.user libidn_PPC.sln libidn_PPC.suo	\
	libidn.sln libidn.suo

EXTRA_DIST = $(SOURCES) $(MSVSC)

CLEANFILES = Libidn.dll

ALL_LOCAL_TARGETS =
INSTALLDIRS_LOCAL_TARGETS = 
INSTALL_DATA_LOCAL_TARGETS =
UNINSTALL_LOCAL_TARGETS =

if CSHARP
ALL_LOCAL_TARGETS += Libidn.dll
INSTALLDIRS_LOCAL_TARGETS += install-dll-dir
INSTALL_DATA_LOCAL_TARGETS += install-dll
UNINSTALL_LOCAL_TARGETS += uninstall-dll

Libidn.dll: $(SOURCES)
	$(CSHARPCOMP) $(CSHARPCOMPFLAGS) -o $@ \
		`for src in $(SOURCES); do echo $(srcdir)/$$src; done`

install-dll-dir:
	$(mkinstalldirs) $(DESTDIR)$(libdir)

install-dll: install-dll-dir
	$(INSTALL_DATA) Libidn.dll $(DESTDIR)$(libdir)/Libidn.dll

uninstall-dll:
	rm -f $(DESTDIR)$(libdir)/Libidn.dll
endif

all-local: $(ALL_LOCAL_TARGETS)
install-data-local: $(INSTALL_DATA_LOCAL_TARGETS)
installdirs-local: $(INSTALLDIRS_LOCAL_TARGETS)
uninstall-local: $(UNINSTALL_LOCAL_TARGETS)
