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
# $Header: //depot/main/gfx/lib/glu/libnurbs/internals/GNUmakefile#12 $

include $(ROOT)/usr/include/make/commondefs

TARGET = libinternals.a

TARGETS = $(TARGET)

LCXXDEFS = -DNDEBUG -DLIBRARYBUILD
LCXXINCS = -I../nurbtess -I../interface -I../../include

HFILES = \
	arc.h \
	arcsorter.h \
	arctess.h \
	backend.h \
	basiccrveval.h \
	basicsurfeval.h \
	bezierarc.h \
	bin.h \
	bufpool.h \
	cachingeval.h \
	coveandtiler.h \
	curve.h \
	curvelist.h \
	defines.h \
	displaylist.h \
	displaymode.h \
	flist.h \
	flistsorter.h \
	glimports.h \
	gridline.h \
	gridtrimvertex.h \
	gridvertex.h \
	hull.h \
	jarcloc.h \
	knotvector.h \
	mapdesc.h \
	maplist.h \
	mesher.h \
	monotonizer.h \
	myassert.h \
	mymath.h \
	mysetjmp.h \
	mystdio.h \
	mystdlib.h \
	mystring.h \
	nurbsconsts.h \
	nurbstess.h \
	patch.h \
	patchlist.h \
	pwlarc.h \
	quilt.h \
	reader.h \
	renderhints.h \
	simplemath.h \
	slicer.h \
	sorter.h \
	subdivider.h \
	trimline.h \
	trimregion.h \
	trimvertex.h \
	trimvertpool.h \
	types.h \
	uarray.h \
	varray.h \
	dataTransform.h \
	$(NULL)

CCFILES = \
	arc.cc \
	arcsorter.cc \
	arctess.cc \
	backend.cc \
	basiccrveval.cc \
	basicsurfeval.cc \
	bin.cc \
	bufpool.cc \
	cachingeval.cc \
	ccw.cc \
	coveandtiler.cc \
	curve.cc \
	curvelist.cc \
	curvesub.cc \
	displaylist.cc \
	flist.cc \
	flistsorter.cc \
	hull.cc \
	intersect.cc \
	knotvector.cc \
	mapdesc.cc \
	mapdescv.cc \
	maplist.cc \
	mesher.cc \
	monotonizer.cc \
	mycode.cc \
	nurbsinterfac.cc \
	nurbstess.cc \
	patch.cc \
	patchlist.cc \
	quilt.cc \
	reader.cc \
	renderhints.cc \
	slicer.cc \
	sorter.cc \
	splitarcs.cc \
	subdivider.cc \
	tobezier.cc \
	trimline.cc \
	trimregion.cc \
	trimvertpool.cc \
	uarray.cc \
	varray.cc \
	dataTransform.cc \
	monoTriangulationBackend.cc \
	$(NULL)

default libs libs_install install: $(TARGET)

headers headers_install apps:

$(TARGET): $(OBJECTS)
	$(AR) crl $@ $(OBJECTS);

include $(COMMONRULES)
