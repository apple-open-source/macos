## $Id: Makefile.am,v 1.1.1.2 2004/12/22 23:49:36 jlovell Exp $
## Copyright (C) 2000 Roger Leigh
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

AUTOMAKE_OPTIONS = 1.4 gnu

@SET_MAKE@

MAINT_CHARSET = latin1

include $(top_srcdir)/scripts/global.mk


## Variables

m4datadir = $(datadir)/aclocal
pkgconfigdatadir = $(prefix)/lib/pkgconfig


## Programs

if BUILD_LIBGIMPPRINTUI2
lib_LTLIBRARIES = libgimpprintui2.la
endif

libgimpprintui2_la_SOURCES =			\
	printrcy.y				\
	printrcl.l				\
	printrc.h				\
	panel.c					\
	plist.c					\
	ui-utils.c				\
	print-image-thumbnail.c			\
	curve.c					\
	gammacurve.c				\
	typebuiltins.c				\
	gimp-print-ui-internal.h

libgimpprintui2_la_LIBADD = $(GIMPPRINTUI2_LIBDEPS)
# Uncommment to build an unversioned library (version in soname)
libgimpprintui2_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS) $(GIMPPRINT_CFLAGS) $(GTK2_CFLAGS)
libgimpprintui2_la_LDFLAGS = -release $(GIMPPRINT_VERSION) -rpath $(libdir)
# Uncomment to build a versioned library
#libgimpprintui2_la_LDFLAGS = \
# -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
# -rpath $(libdir)

AM_LFLAGS = -i
AM_YFLAGS = -d

printrcl.o: printrcy.o
plist.o: printrcy.o

## Data

pkgconfigdata_DATA = gimpprintui2.pc


## Rules

# Copied from include/gimp-print-ui2/Makefile.am
# This duplication needs fixing somehow
stpui_public_h_sources = \
	$(top_srcdir)/include/gimp-print-ui2/gimp-print-ui.h	\
	$(top_srcdir)/include/gimp-print-ui2/curve.h		\
	$(top_srcdir)/include/gimp-print-ui2/gammacurve.h

# Really need to depend upon $(stpui_public_h_sources) here...
typebuiltins.c: Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include <gimp-print-ui2/gimp-print-ui.h>" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(stpui_public_h_sources) ) > $@


## Clean

MAINTAINERCLEANFILES = Makefile.in printrcl.c printrcy.c printrcy.h
