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

# NOTES:
# - The file "lor-c.mn" contains the source for the List of Routines section
#   of the C edition of the GL Reference Guide.  The usual targets
#   lor-c.p, lor-c.ps, and lor-c.psv exist for it; however, it does not have
#   a .3g suffix because we don't want to include it as a on-line man page.
#   The file should be generated!!!

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs

DEPTH = ../..
LANGOPT = f
PREFIX = gl
IDB_PATH = /usr/catman/g_man/cat3/standard

MANPREF = man
include $(DEPTH)/mandefs

APIFILE = $(ROOT)/usr/include/GL/gl.h

VERSION     = Version 1.2
DATE	    = 10 Nov 1998
RELEASE     = 1.0
MMFLAGS     = -rs2

LEGALSUF1   = gl#
LEGALSUF2   = gl#
LEGALSUF3   = gl#
LEGALSUF4   = gl#

LOCALPRINTLISTFILTER = | sed -e '/^intro\.p$$/d' | (echo intro.p ; cat )

default:
	$(MAKE) links
	$(MAKE) manpages

headers headers_install libs libs_install install apps:

distsi: default
	$(MAKE) $(COMMONPREF)$@

distoss:
	$(MAKE) $(COMMONPREF)$@

links:
	cd ../standard ; \
	for i in *.gl ; do \
	    ( cd ../ftn ; if test ! -L fgl$$i ; then \
	      /bin/ln -s ../standard/$$i fgl$$i; fi ); \
	done

manpages: $(patsubst %.gl,%.3gl,$(wildcard *.gl))

const.diff: const.awk *.gl $(ROOT)/usr/include/GL/gl.h
	awk -f const.awk $(ROOT)/usr/include/GL/gl.h *.gl | \
	sort | uniq > const.diff

clean:
	rm -f *.gl

clobber: clean
	rm -f *.3gl

include $(DEPTH)/manrules
include $(COMMONRULES)
