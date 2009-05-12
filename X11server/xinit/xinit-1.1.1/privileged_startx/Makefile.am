# Copyright (c) 2008 Apple Inc.
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
# HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
# Except as contained in this notice, the name(s) of the above
# copyright holders shall not be used in advertising or otherwise to
# promote the sale, use or other dealings in this Software without
# prior written authorization.

include $(top_srcdir)/cpprules.in

xinitrcdir = $(libdir)/X11/xinit
privstartxdir = $(xinitrcdir)/privileged_startx.d

xinitrc_PROGRAMS = privileged_startx
privstartx_SCRIPTS = 10-tmpdirs 20-font_cache

AM_CPPFLAGS = -DXINITDIR=\"$(xinitrcdir)\" -DSCRIPTDIR=\"$(privstartxdir)\" -DBINDIR=\"$(bindir)\"
CPP_FILES_FLAGS = -DXINITDIR="$(xinitrcdir)" -DSCRIPTDIR="$(privstartxdir)" -DBINDIR="$(bindir)"

if TIGER_LAUNCHD
CPP_FILES_FLAGS += -DTIGER_LAUNCHD
endif

dist_privileged_startx_SOURCES = \
	server.c \
	client.c \
	privileged_startx.c

nodist_privileged_startx_SOURCES = \
	privileged_startxServer.c \
	privileged_startxUser.c

BUILT_SOURCES = \
	privileged_startxServer.c \
	privileged_startxUser.c \
	privileged_startxServer.h \
	privileged_startx.h

launchdaemons_PRE = org.x.privileged_startx.plist.cpp
launchdaemons_DATA = $(launchdaemons_PRE:plist.cpp=plist)

10-tmpdirs: 10-tmpdirs.cpp Makefile
20-font_cache: 20-font_cache.cpp Makefile

CLEANFILES = \
	$(privstartx_SCRIPTS) \
	$(BUILT_SOURCES) \
	$(launchdaemons_DATA)

EXTRA_DIST = \
	10-tmpdirs.cpp \
	20-font_cache.cpp \
	org.x.privileged_startx.plist.cpp \
	privileged_startx.defs \
	privileged_startx_types.h

$(BUILT_SOURCES): $(srcdir)/privileged_startx.defs
	mig -sheader privileged_startxServer.h $(srcdir)/privileged_startx.defs
