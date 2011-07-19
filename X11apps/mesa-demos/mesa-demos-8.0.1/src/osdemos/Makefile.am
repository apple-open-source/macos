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
	$(OSMESA_CFLAGS) \
	-I$(top_srcdir)/src/util

if HAVE_OSMESA
bin_PROGRAMS = \
	osdemo \
	osdemo16 \
	osdemo32
endif

osdemo16_SOURCES = osdemo.c
osdemo32_SOURCES = osdemo.c
osdemo_LDADD = $(OSMESA_LIBS) ../util/libutil.la
osdemo16_LDADD = $(OSMESA_LIBS) $(OSMESA16_LIBS) ../util/libutil.la
osdemo32_LDADD = $(OSMESA32_LIBS) ../util/libutil.la
