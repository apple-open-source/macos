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
	$(X11_CFLAGS) \
	-I$(top_srcdir)/src/util
AM_LDFLAGS = \
	$(DEMO_LIBS) \
	$(X11_LIBS)

if HAVE_X11
noinst_LTLIBRARIES = libpbutil.la

bin_PROGRAMS = \
	corender \
	glsync \
	glthreads \
	glxdemo \
	glxgears \
	glxgears_fbconfig \
	glxgears_pixmap \
	glxcontexts \
	glxheads \
	glxinfo \
	glxpixmap \
	glxpbdemo \
	glxsnoop \
	glxswapcontrol \
	manywin \
	multictx \
	offset \
	overlay \
	pbinfo \
	pbdemo \
	shape \
	sharedtex \
        sharedtex_mt \
	texture_from_pixmap \
	wincopy \
	xfont \
	xrotfontdemo
endif

libpbutil_la_SOURCES = \
	pbutil.c \
	pbutil.h

corender_SOURCES = \
	corender.c \
	ipc.c \
	ipc.h

xrotfontdemo_SOURCES = \
	xrotfontdemo.c \
	xuserotfont.c \
	xuserotfont.h

glthreads_LDADD = -lpthread
glxgears_fbconfig_LDADD = libpbutil.la
pbdemo_LDADD = libpbutil.la
pbinfo_LDADD = libpbutil.la
sharedtex_mt_LDADD = -lpthread

EXTRA_DIST = \
	yuvrect_client.c \
	xdemo.c
