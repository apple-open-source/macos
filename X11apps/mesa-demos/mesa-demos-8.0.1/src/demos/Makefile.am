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

AM_CFLAGS = \
	$(DEMO_CFLAGS) \
	$(GLUT_CFLAGS) \
	-I$(top_srcdir)/src/util
AM_LDFLAGS = \
	$(DEMO_LIBS) \
	$(GLUT_LIBS)

if HAVE_GLUT
bin_PROGRAMS = \
	arbfplight \
	arbfslight \
	arbocclude \
	bounce \
	clearspd \
	copypix \
	cubemap \
	dinoshade \
	dissolve \
	drawpix \
	engine \
	fbo_firecube \
	fbotexture \
	fire \
	fogcoord \
	fplight \
	fslight \
	gamma \
	gearbox \
	gears \
	geartrain \
	glinfo \
	gloss \
	gltestperf \
	ipers \
	isosurf \
	lodbias \
	morph3d \
	multiarb \
	paltex \
	pointblast \
	projtex \
	ray \
	readpix \
	reflect \
	renormal \
	shadowtex \
	singlebuffer \
	spectex \
	spriteblast \
	stex3d \
	teapot \
	terrain \
	tessdemo \
	texcyl \
	texenv \
	textures \
	trispd \
	tunnel2 \
	tunnel \
	vao_demo \
	winpos
endif

tunnel_SOURCES = \
	tunnel.c \
	tunneldat.h

copypix_LDADD = ../util/libutil.la
cubemap_LDADD = ../util/libutil.la
drawpix_LDADD = ../util/libutil.la
dissolve_LDADD = ../util/libutil.la
engine_LDADD = ../util/libutil.la
fbo_firecube_LDADD = ../util/libutil.la
fire_LDADD = ../util/libutil.la
gloss_LDADD = ../util/libutil.la
ipers_LDADD = ../util/libutil.la
isosurf_LDADD = ../util/libutil.la
lodbias_LDADD = ../util/libutil.la
multiarb_LDADD = ../util/libutil.la
projtex_LDADD = ../util/libutil.la
readpix_LDADD = ../util/libutil.la
reflect_LDADD = ../util/libutil.la
teapot_LDADD = ../util/libutil.la
texcyl_LDADD = ../util/libutil.la
textures_LDADD = ../util/libutil.la
tunnel_LDADD = ../util/libutil.la
tunnel2_LDADD = ../util/libutil.la
winpos_LDADD = ../util/libutil.la

EXTRA_DIST = \
	README \
	geartrain.dat \
	isosurf.dat \
	terrain.dat
