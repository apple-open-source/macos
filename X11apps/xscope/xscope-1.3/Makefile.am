#
# Copyright 2008-2009 Sun Microsystems, Inc.  All rights reserved.
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
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
#

bin_PROGRAMS = xscope

AM_CFLAGS = $(CWARNFLAGS) $(XSCOPE_CFLAGS)
xscope_LDADD = $(XSCOPE_LIBS)

xscope_SOURCES =	\
	audio.c \
	bigreqscope.h \
	common.c \
	decode11.c \
	decode_bigreq.c \
	decode_glx.c \
	decode_lbx.c \
	decode_randr.c \
	decode_render.c \
	decode_shm.c \
	decode_wcp.c \
	decodenas.c \
	extensions.c \
	extensions.h \
	fd.c \
	fd.h \
	glxscope.h \
	lbxscope.h \
	nas.h \
	print11.c \
	print_bigreq.c \
	print_glx.c \
	print_lbx.c \
	print_randr.c \
	print_render.c \
	print_shm.c \
	print_wcp.c \
	printnas.c \
	proto.h \
	prtype.c \
	randrscope.h \
	renderscope.h \
	scope-transport.c \
	scope.c \
	scope.h \
	server.c \
	shmscope.h \
	table11.c \
	wcpscope.h \
	x11.h

appman_PRE = \
        xscope.man

appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST = $(appman_PRE) autogen.sh
MAINTAINERCLEANFILES = ChangeLog INSTALL
CLEANFILES = $(appman_DATA)

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

SUFFIXES = .$(APP_MAN_SUFFIX) .man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
.man.$(APP_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

if LINT
# Check source code with tools like lint & sparse

ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS)

lint:	$(BUILT_SOURCES)
	@for f in $(xscope_SOURCES) ; do \
	    case $$f in *.c) \
		$(LINT) $(ALL_LINT_FLAGS) $(srcdir)/$$f ;; esac ; \
        done
endif LINT
