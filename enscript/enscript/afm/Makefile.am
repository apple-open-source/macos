#
# Automakefile for the AFM files.
# Copyright (c) 1995-1999 Markku Rossi
#
# Author: Markku Rossi <mtr@iki.fi>
#

#
# This file is part of GNU enscript.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

default_afm = agd.afm agdo.afm agw.afm agwo.afm bkd.afm bkdi.afm	\
bkl.afm bkli.afm cob.afm cobo.afm com.afm coo.afm hv.afm hvb.afm	\
hvbo.afm hvc.afm hvcb.afm hvcbo.afm hvcdo.afm hvn.afm hvnb.afm		\
hvnbo.afm hvno.afm hvo.afm ncb.afm ncbi.afm nci.afm ncr.afm pob.afm	\
pobi.afm poi.afm por.afm sy.afm tib.afm tibi.afm tii.afm tir.afm	\
zcmi.afm zd.afm

public_fonts = matrix.afm matrix.pfa

EXTRA_DIST = font.map $(default_afm) $(public_fonts)

install-data-local:
	$(top_srcdir)/mkinstalldirs $(datadir)/enscript
	$(top_srcdir)/mkinstalldirs $(datadir)/enscript/afm
	for f in $(EXTRA_DIST); do \
	  $(INSTALL_DATA) $(srcdir)/$$f $(datadir)/enscript/afm/$$f; \
	done

uninstall-local:
	rm -rf $(datadir)/enscript/afm
