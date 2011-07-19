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

if HAVE_GLUT
noinst_PROGRAMS = \
	tri-tex \
	point-position \
	tri-depth \
	tri-depth2 \
	tri-depthwrite \
	tri-depthwrite2 \
	tri-param \
	fp-tri
endif

EXTRA_DIST = \
	abs.txt \
	add-sat.txt \
	add-swz.txt \
	add.txt \
	cmp.txt \
	cos.txt \
	dp3.txt \
	dp4.txt \
	dph.txt \
	dst.txt \
	ex2.txt \
	flr.txt \
	frc.txt \
	kil-pos.txt \
	kil-texcoord-sgt.txt \
	kil.txt \
	lg2.txt \
	lit.txt \
	local.txt \
	lrp.txt \
	mad.txt \
	max.txt \
	min.txt \
	mov-alias.txt \
	mov.txt \
	mul-alias.txt \
	mul-swz.txt \
	mul.txt \
	negate.txt \
	position.txt \
	pow.txt \
	rcp.txt \
	rsq.txt \
	scs.txt \
	sge2.txt \
	sge.txt \
	sin.txt \
	slt.txt \
	sub.txt \
	swz2.txt \
	swz3.txt \
	swz.txt \
	tex-pos-kil-1.txt \
	tex-pos-kil.txt \
	tex-pos.txt \
	tex.txt \
	xpd.txt
