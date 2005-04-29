## $Id: Makefile.am,v 1.1.1.2 2004/07/23 06:26:28 jlovell Exp $
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


## Data

# Conditional installation of manpages is currently broken in automake...

if BUILD_CUPS
CUPS_MAN = \
	cups-calibrate.8 \
	cups-genppd.8 \
	cups-genppdconfig.8 \
	cups-genppdupdate.8
endif

if BUILD_ESCPUTIL
ESCPUTIL_MAN = escputil.1
endif

if BUILD_GHOSTSCRIPT
GHOSTSCRIPT_MAN = ijsgimpprint.1
endif

MAIN_MAN = \
	gimpprint.3 \
	gimpprint-color.7 \
	gimpprint-dithers.7 \
	gimpprint-imagetypes.7 \
	gimpprint-inktypes.7 \
	gimpprint-mediasizes.7 \
	gimpprint-mediasources.7 \
	gimpprint-mediatypes.7 \
	gimpprint-models.7 \
	gimpprint-resolutions.7

man_MANS = $(CUPS_MAN) $(ESCPUTIL_MAN) $(GHOSTSCRIPT_MAN) $(MAIN_MAN)

EXTRA_man_MANS = \
	cups-calibrate.8 \
	cups-genppd.8 \
	cups-genppdconfig.8 \
	cups-genppdupdate.8 \
	escputil.1 \
	ijsgimpprint.1


## Clean

DISTCLEANFILES = $(MAIN_MAN) $(EXTRA_man_MANS)
MAINTAINERCLEANFILES = Makefile.in

