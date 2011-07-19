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
	$(VG_CFLAGS) \
	$(EGL_CFLAGS) \
	$(X11_CFLAGS) \
	-I$(top_srcdir)/src/egl/eglut

AM_LDFLAGS = \
	$(VG_LIBS) \
	$(EGL_LIBS) \
	-lm

if HAVE_X11
EGL_X11_DEMOS = \
	lion_x11 \
	sp_x11
endif

if HAVE_EGL
if HAVE_VG
noinst_PROGRAMS = \
	lion_screen \
	sp_screen \
	$(EGL_X11_DEMOS)
endif
endif

lion_screen_SOURCES = lion.c lion-render.c
sp_screen_SOURCES = sp.c

lion_x11_SOURCES = lion.c lion-render.c
sp_x11_SOURCES = sp.c

lion_screen_LDADD = ../eglut/libeglut_screen.la
sp_screen_LDADD = ../eglut/libeglut_screen.la

lion_x11_LDADD = ../eglut/libeglut_x11.la
sp_x11_LDADD = ../eglut/libeglut_x11.la

SUBDIRS = \
	trivial
