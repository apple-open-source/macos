#!gmake
#
# License Applicability. Except to the extent portions of this file are
# made subject to an alternative license as permitted in the SGI Free
# Software License B, Version 1.1 (the "License"), the contents of this
# file are subject only to the provisions of the License. You may not use
# this file except in compliance with the License. You may obtain a copy
# of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
# Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
#
# http://oss.sgi.com/projects/FreeB
#
# Note that, as provided in the License, the Software is distributed on an
# "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
# DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
# CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
# PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
#
# Original Code. The Original Code is: OpenGL Sample Implementation,
# Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
# Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
# Copyright in any portions created by third parties is as indicated
# elsewhere herein. All Rights Reserved.
#
# Additional Notice Provisions: The application programming interfaces
# established by SGI in conjunction with the Original Code are The
# OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
# April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
# 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
# Window System(R) (Version 1.3), released October 19, 1998. This software
# was created using the OpenGL(R) version 1.2.1 Sample Implementation
# published by SGI, but has not been independently verified as being
# compliant with the OpenGL(R) version 1.2.1 Specification.
#
# $Date$ $Revision$
# $Header: //depot/main/gfx/lib/glu/GNUmakefile#17 $

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/libspec/libspecdefs

TARGET = libGLU.so.1

TARGETS = $(TARGET)

LLDLIBS = -lGL -lm
ifeq ($(OS),Linux)
# use g++ rather than ld
LD=g++
LDDSOFLAGS=-shared -Wl,-soname=$(TARGET) -Wl,--whole-archive
LDFLAGS=-Wl,--no-whole-archive
else
LLDLIBS += -lc -lC
endif
DSONAME = $(TARGET)
DSOEXPORTS = glu.exports

SUBDIRS = \
	include \
	libutil \
	libtess \
	libnurbs \
	$(NULL)

UTIL = libutil/libutil.a
TESS = libtess/libtess.a
NURB_INTERNAL = libnurbs/internals/libinternals.a
NURB_INTERFACE = libnurbs/interface/libinterface.a
NURB_TESS = libnurbs/nurbtess/libnurbtess.a
SRCLIBS = $(UTIL) $(TESS) $(NURB_INTERNAL) $(NURB_INTERFACE) $(NURB_TESS)

default libs: $(_FORCE)
	cd libutil; $(MAKE);
	cd libtess; $(MAKE);
	cd libnurbs/internals; $(MAKE);
	cd libnurbs/interface; $(MAKE);
	cd libnurbs/nurbtess; $(MAKE);
	$(MAKE) $(TARGET);

headers headers_install apps:

libs_install install: default
	$(INSTALL) -F $(LIB_LOCATION) $(TARGET);

distsi distoss:
	$(MAKE) $(COMMONPREF)$@
	$(SUBDIRS_MAKERULE)

$(TARGET): $(DSOEXPORTS) $(SRCLIBS)
	$(LD) $(LDDSOFLAGS) $(SRCLIBS) $(LDFLAGS) -o $@;

$(DSOEXPORTS): $(LIBSPEC_PARSER) $(TYPEMAPSDIR)/$(C_TYPEMAP) \
	       $(SPECDIR)/$(GLU_SPEC) exports.ls GNUmakefile
	rm -f $@.tmp.ls;
	cat exports.ls > $@.tmp.ls;
	$(LIBSPEC) -DOUTPUT="$@" -DPROCPREFIX="glu" \
		   -DTYPEMAP=$(TYPEMAPSDIR)/$(C_TYPEMAP) \
		   -P $(LIBSPEC_PARSER) $@.tmp.ls "$(SPECDIR)/$(GLU_SPEC)" > $@;
	sort +1 $@ -o $@;
	rm $@.tmp.ls;

# local dirt, cpp output files and assembler listings
LDIRT = $(DSOEXPORTS)

include $(COMMONRULES)

$(COMMONTARGS): % : $(COMMONPREF)%
	$(SUBDIRS_MAKERULE)
