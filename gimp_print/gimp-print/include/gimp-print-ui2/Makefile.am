## $Id: Makefile.am,v 1.1.1.2 2004/12/22 23:49:25 jlovell Exp $
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


pkgincludedir = $(includedir)/gimp-print-ui2


## Data

stpui_public_h_sources =	\
	gimp-print-ui.h		\
	curve.h			\
	gammacurve.h

stpui_built_public_sources = \
	typebuiltins.h


pkginclude_HEADERS = \
	$(stpui_public_h_sources) \
	$(stpui_built_public_sources)


## Rules

typebuiltins.h: $(stpui_public_h_sources) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef STPUI_TYPE_BUILTINS_H\n#define STPUI_TYPE_BUILTINS_H\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define STPUI_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* STPUI_TYPE_BUILTINS_H */" \
		$(stpui_public_h_sources) ) > $@


## Clean

MAINTAINERCLEANFILES = Makefile.in
