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
	clear \
	clear-fbo \
	clear-fbo-tex \
	clear-random \
	clear-repeat \
	clear-scissor \
	clear-undefined \
	createwin \
	dlist-begin-call-end \
	dlist-dangling \
	dlist-degenerate \
	dlist-edgeflag \
	dlist-edgeflag-dangling \
	dlist-flat-tri \
	dlist-mat-tri \
	dlist-recursive-call \
	dlist-tri-flat-tri \
	dlist-tri-mat-tri \
	draw2arrays \
	drawarrays \
	drawelements \
	drawrange \
	flat-clip \
	fs-tri \
	line \
	line-clip \
	line-cull \
	line-flat \
	lineloop \
	lineloop-clip \
	lineloop-elts \
	line-smooth \
	line-stipple-wide \
	linestrip \
	linestrip-clip \
	linestrip-flat-stipple \
	linestrip-stipple \
	linestrip-stipple-wide \
	line-userclip \
	line-userclip-clip \
	line-userclip-nop \
	line-userclip-nop-clip \
	line-wide \
	long-fixed-func \
	pgon-mode \
	point \
	point-clip \
	point-param \
	point-sprite \
	point-wide \
	point-wide-smooth \
	poly \
	poly-flat \
	poly-flat-clip \
	poly-flat-unfilled-clip \
	poly-unfilled \
	quad \
	quad-clip-all-vertices \
	quad-clip \
	quad-clip-nearplane \
	quad-degenerate \
	quad-flat \
	quad-offset-factor \
	quad-offset-unfilled \
	quad-offset-units \
	quads \
	quadstrip \
	quadstrip-clip \
	quadstrip-cont \
	quadstrip-flat \
	quad-tex-2d \
	quad-tex-3d \
	quad-tex-alpha \
	quad-tex-pbo \
	quad-tex-sub \
	quad-unfilled \
	quad-unfilled-clip \
	quad-unfilled-stipple \
	readpixels \
	tri-alpha \
	tri-alpha-tex \
	tri-array-interleaved \
	tri-blend \
	tri-blend-color \
	tri-blend-max \
	tri-blend-min \
	tri-blend-revsub \
	tri-blend-sub \
	tri \
	tri-clear \
	tri-clip \
	tri-cull-both \
	tri-cull \
	tri-dlist \
	tri-edgeflag \
	trifan \
	trifan-flat \
	trifan-flat-clip \
	trifan-flat-unfilled-clip \
	trifan-unfilled \
	tri-fbo \
	tri-fbo-tex \
	tri-fbo-tex-mip \
	tri-flat \
	tri-flat-clip \
	tri-fog \
	tri-fp \
	tri-fp-const-imm \
	tri-lit \
	tri-lit-material \
	tri-logicop-none \
	tri-logicop-xor \
	tri-mask-tri \
	tri-multitex-vbo \
	tri-orig \
	tri-query \
	tri-repeat \
	tri-scissor-tri \
	tri-square \
	tri-stencil \
	tri-stipple \
	tristrip \
	tristrip-clip \
	tristrip-flat \
	tri-tex-3d \
	tri-tex \
	tri-tri \
	tri-unfilled \
	tri-unfilled-clip \
	tri-unfilled-edgeflag \
	tri-unfilled-fog \
	tri-unfilled-point \
	tri-unfilled-smooth \
	tri-unfilled-tri \
	tri-unfilled-tri-lit \
	tri-unfilled-userclip \
	tri-unfilled-userclip-stip \
	tri-userclip \
	tri-viewport \
	tri-z-9 \
	tri-z \
	tri-z-eq \
	vbo-drawarrays \
	vbo-drawelements \
	vbo-drawrange \
	vbo-noninterleaved \
	vp-array \
	vp-array-int \
	vp-clip \
	vp-line-clip \
	vp-tri \
	vp-tri-cb \
	vp-tri-cb-pos \
	vp-tri-cb-tex \
	vp-tri-imm \
	vp-tri-invariant \
	vp-tri-swap \
	vp-tri-tex \
	vp-unfilled
endif
