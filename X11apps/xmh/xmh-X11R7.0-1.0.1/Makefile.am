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

bin_PROGRAMS = xmh

xmh_CFLAGS = $(XMH_CFLAGS) -DXVENDORNAME=\"X.org\ Foundation\" -DXORG_RELEASE=\"$(VERSION)\" -D_BSD_SOURCE
xmh_LDADD = $(XMH_LIBS)

xmh_SOURCES =	\
        actions.h \
        bbox.c \
        bbox.h \
        bboxint.h \
        command.c \
        compfuncs.c \
        externs.h \
        folder.c \
        globals.h \
        init.c \
        main.c \
        menu.c \
        miscfuncs.c \
        mlist.c \
        mlist.h \
        msg.c \
        msg.h \
        pick.c \
        popup.c \
        screen.c \
        toc.c \
        tocfuncs.c \
        toc.h \
        tocintrnl.h \
        tocutil.c \
        tocutil.h \
        tsource.c \
        tsource.h \
        tsourceP.h \
        util.c \
        version.h \
        viewfuncs.c \
        xmh.h

appman_PRE = \
        xmh.man

# App default files  (*.ad)

appdefaultdir = @appdefaultdir@


APPDEFAULTFILES = \
        Xmh

SUFFIXES = .ad

.ad:
	cp $< $@

appdefault_DATA = $(APPDEFAULTFILES)

# bitmap files

bitmapdir = $(includedir)/X11/bitmaps

dist_bitmap_DATA = black6 box6

#

EXTRA_DIST = $(APPDEFAULTFILES:%=%.ad) Xmh.sample

CLEANFILES = $(APPDEFAULTFILES)


appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST += $(appman_PRE)
CLEANFILES += $(appman_DATA)

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
