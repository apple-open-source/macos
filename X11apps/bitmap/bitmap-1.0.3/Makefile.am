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

bin_PROGRAMS = bitmap bmtoa atobm

# bitmap

bitmap_CFLAGS = $(BITMAP_CFLAGS)
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

# bmtoa

bmtoa_CFLAGS = $(BMTOA_CFLAGS)
bmtoa_LDADD = $(BMTOA_LIBS)
bmtoa_SOURCES = \
         bmtoa.c

# atobm

atobm_CFLAGS = $(ATOBM_CFLAGS)
atobm_LDADD = $(ATOBM_LDADD)
atobm_SOURCES = \
         atobm.c

# mans

appman_PRE = \
             bitmap.man


# app default files

# App default files  (*.ad)

appdefaultdir = @appdefaultdir@

APPDEFAULTFILES = \
	app-defaults/Bitmap-color \
	app-defaults/Bitmap-nocase \
	app-defaults/Bitmap

SUFFIXES = .ad

.ad:
	cp $< $@

appdefault_DATA = $(APPDEFAULTFILES)


EXTRA_DIST = $(APPDEFAULTFILES:%=%.ad) bitmap.icon ChangeLog autogen.sh
CLEANFILES = $(APPDEFAULTFILES)
MAINTAINERCLEANFILES=ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

# bitmaps

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

APP_MAN_DIR_SUFFIX = $(APP_MAN_DIR:@mandir@/man%=%)

appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@) \
	      $(man_shadows:=.@APP_MAN_SUFFIX@)

BUILT_SOURCES = shadows.DONE

EXTRA_DIST += $(appman_PRE)
CLEANFILES += $(appman_DATA) $(BUILT_SOURCES)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = \
	-e 's|__vendorversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xorgversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xservername__|Xorg|g' \
	-e 's|__xconfigfile__|xorg.conf|g' \
	-e 's|__projectroot__|$(prefix)|g' \
	-e 's|__apploaddir__|$(appdefaultdir)|' \
	-e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
	-e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
	-e 's|__adminmansuffix__|$(ADMIN_MAN_SUFFIX)|g' \
	-e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g' \
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g'

SUFFIXES += .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@

man_shadows = atobm bmtoa

shadows.DONE:
	-rm -f $(man_shadows:=.@APP_MAN_SUFFIX@)
	(for i in $(man_shadows:=.@APP_MAN_SUFFIX@) ; do \
	 echo .so man$(APP_MAN_DIR_SUFFIX)/bitmap.$(APP_MAN_SUFFIX) > $$i; \
	 done)
