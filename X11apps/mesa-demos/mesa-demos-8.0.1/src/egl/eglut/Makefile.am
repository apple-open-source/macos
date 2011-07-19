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

AM_CFLAGS = $(EGL_CFLAGS)
AM_LDFLAGS = $(EGL_LIBS)

if HAVE_EGL
if HAVE_X11
eglut_x11 = libeglut_x11.la
endif
noinst_LTLIBRARIES = libeglut_screen.la $(eglut_x11)
endif

libeglut_screen_la_SOURCES = \
	eglut.c \
	eglut.h \
	eglutint.h \
	eglut_screen.c

libeglut_x11_la_SOURCES = \
	eglut.c \
	eglutint.h \
	eglut_x11.c
libeglut_x11_la_CFLAGS = $(X11_CFLAGS) $(EGL_CFLAGS)
libeglut_x11_la_LIBADD = $(X11_LIBS) $(EGL_LIBS)
