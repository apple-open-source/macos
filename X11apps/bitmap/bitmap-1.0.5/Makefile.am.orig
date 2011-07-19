# 
#  Copyright 2005  Red Hat, Inc.
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Red Hat makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
# 
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

SUBDIRS=man
bin_PROGRAMS = bitmap bmtoa atobm

bitmap_CFLAGS = $(CWARNFLAGS) $(BITMAP_CFLAGS)
bitmap_LDADD = $(BITMAP_LIBS) -lm
bitmap_SOURCES = \
         BitEdit.c \
         CutPaste.c \
         Graphics.c \
         ReqMach.c \
         Bitmap.c \
         Dialog.c \
         Handlers.c \
         Bitmap.h \
         BitmapP.h \
         Dialog.h \
         Requests.h

bmtoa_CFLAGS = $(CWARNFLAGS) $(BMTOA_CFLAGS)
bmtoa_LDADD = $(BMTOA_LIBS)
bmtoa_SOURCES = bmtoa.c

atobm_CFLAGS = $(CWARNFLAGS) $(ATOBM_CFLAGS)
atobm_LDADD = $(ATOBM_LDADD)
atobm_SOURCES = atobm.c

EXTRA_DIST = autogen.sh

bitmapdir = $(includedir)/X11/bitmaps
dist_bitmap_DATA =	\
	Dashes		\
	Down		\
	Excl		\
	FlipHoriz	\
	FlipVert	\
	Fold		\
	Left		\
	Right		\
	RotateLeft	\
	RotateRight	\
	Stipple		\
	Term		\
	Up

# App default files  (*.ad)
DISTCHECK_CONFIGURE_FLAGS = --with-appdefaultdir=\$${datadir}/X11/app-defaults

appdefaultdir = @appdefaultdir@
dist_appdefault_DATA = \
	app-defaults/Bitmap-color \
	app-defaults/Bitmap-nocase \
	app-defaults/Bitmap

MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
