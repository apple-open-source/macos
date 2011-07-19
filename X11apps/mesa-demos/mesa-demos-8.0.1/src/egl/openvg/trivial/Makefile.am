# Copyright © 2010 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# Authors:
#    Eric Anholt <eric@anholt.net>

# These programs aren't intended to be included with the normal distro.
# They're not too interesting but they're good for testing.

AM_CFLAGS = \
	$(X11_CFLAGS) \
	$(EGL_CFLAGS) \
	$(VG_CFLAGS)
AM_LDFLAGS = \
	$(X11_LIBS) \
	$(EGL_LIBS) \
	$(VG_LIBS) \
	-lm

if HAVE_X11
if HAVE_EGL
if HAVE_VG
noinst_LTLIBRARIES = libcommon.la
libcommon_la_SOURCES = \
	eglcommon.c

noinst_PROGRAMS = \
	arc \
	cap \
	clear \
	coord \
	dash \
	ellipse \
	filter \
	gradorigin \
	lineto \
	lingrad \
	lookup \
	mask4 \
	mask \
	path3 \
	radialgrad \
	readpixels \
	roundedrect \
	star-nonzero \
	star-oddeven \
	stroke2 \
	stroke

# VGU is broken on current mesa master
#	vguarc

arc_LDADD = libcommon.la
cap_LDADD = libcommon.la
clear_LDADD = libcommon.la
coord_LDADD = libcommon.la
dash_LDADD = libcommon.la
ellipse_LDADD = libcommon.la
filter_LDADD = libcommon.la
gradorigin_LDADD = libcommon.la
lineto_LDADD = libcommon.la
lingrad_LDADD = libcommon.la
lookup_LDADD = libcommon.la
mask4_LDADD = libcommon.la
mask_LDADD = libcommon.la
path3_LDADD = libcommon.la
radialgrad_LDADD = libcommon.la
readpixels_LDADD = libcommon.la
roundedrect_LDADD = libcommon.la
star_nonzero_LDADD = libcommon.la
star_oddeven_LDADD = libcommon.la
stroke2_LDADD = libcommon.la
stroke_LDADD = libcommon.la
#vguarc_LDADD = libcommon.la
endif
endif
endif
