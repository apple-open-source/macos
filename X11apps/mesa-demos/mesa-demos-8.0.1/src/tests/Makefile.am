# Copyright © 2009 Intel Corporation
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
	$(DEMO_CFLAGS) \
	$(GLUT_CFLAGS) \
	-I$(top_srcdir)/src/util
AM_LDFLAGS = \
	$(DEMO_LIBS) \
	$(GLUT_LIBS)

if HAVE_MESA_SOURCE
GETPROCADDRESS=getprocaddress

getprocaddress_SOURCES = \
	getprocaddress.c
nodist_getprocaddress_SOURCES = \
	getproclist.h

getproclist.h: $(MESA_GLAPI)/gl_API.xml getprocaddress.c getprocaddress.py
	python getprocaddress.py > getproclist.h

BUILT_SOURCES = \
	getproclist.h
endif

if HAVE_GLUT
noinst_PROGRAMS = \
	afsmultiarb \
	antialias \
	arbfpspec \
	arbfptest1 \
	arbfptexture \
	arbfptrig \
	arbgpuprog \
	arbnpot \
	arbnpot-mipmap \
	arbvptest1 \
	arbvptest3 \
	arbvptorus \
	arbvpwarpmesh \
	arraytexture \
	auxbuffer \
	blendxor \
	blitfb \
	bufferobj \
	bumpmap \
	bug_3050 \
	bug_3101 \
	bug_3195 \
	bug_texstore_i8 \
	calibrate_rast \
	condrender \
	copypixrate \
	cva \
	cva_huge \
	cylwrap \
	drawbuffers \
	drawbuffers2 \
	exactrast \
	ext422square \
	floattex \
	fbotest1 \
	fbotest2 \
	fbotest3 \
	fillrate \
	fog \
	fogcoord \
	fptest1 \
	fptexture \
	$(GETPROCADDRESS) \
	getteximage \
	glutfx \
	interleave \
	invert \
	jkrahntest \
	lineclip \
	manytex \
	mapbufrange \
	minmag \
	mipgen \
	mipmap_comp \
	mipmap_comp_tests \
	mipmap_limits \
	mipmap_view \
	multipal \
	multitexarray \
	multiwindow \
	no_s3tc \
	packedpixels \
	pbo \
	persp_hint \
	prim \
	prog_parameter \
	quads \
	random \
	readrate \
	rubberband \
	scissor \
	scissor-viewport \
	seccolor \
	shader_api \
	sharedtex \
	stencilreaddraw \
	stencilwrap \
	streaming_rect \
	subtex \
	subtexrate \
	tex1d \
	texcmp \
	texcompress2 \
	texcompsub \
	texdown \
	texfilt \
	texgenmix \
	texleak \
	texline \
	texobj \
	texobjshare \
	texrect \
	texwrap \
	unfilledclip \
	vparray \
	vpeval \
	vptest1 \
	vptest2 \
	vptest3 \
	vptorus \
	vpwarpmesh \
	yuvrect \
	yuvsquare \
	zbitmap \
	zcomp \
	zdrawpix \
	zreaddraw
endif

afsmultiarb_LDADD = ../util/libutil.la
arraytexture_LDADD = ../util/libutil.la
auxbuffer_LDADD = -lX11
bug_3195_LDADD = ../util/libutil.la
bumpmap_LDADD = ../util/libutil.la
floattex_LDADD = ../util/libutil.la
fillrate_LDADD = ../util/libutil.la
invert_LDADD = ../util/libutil.la
jkrahntest_LDADD = -lX11
mipmap_limits_LDADD = ../util/libutil.la
mipmap_view_LDADD = ../util/libutil.la
sharedtex_LDADD = -lX11
texobjshare_LDADD = -lX11
texrect_LDADD = ../util/libutil.la

EXTRA_DIST = \
	Makefile.am \
	getprocaddress.py \
	texcomp_image.h \
	tkmap.c
