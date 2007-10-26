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

bin_PROGRAMS = xman

helpdir=@HELPDIR@
dist_help_DATA = xman.help

xman_CFLAGS = $(XMAN_CFLAGS) -D_BSD_SOURCE -DHELPFILE=\"$(helpdir)/xman.help\"
xman_LDADD = $(XMAN_LIBS)

xman_SOURCES =	\
        buttons.c \
        defs.h \
        globals.c \
        globals.h \
        handler.c \
        help.c \
        iconclosed.h \
        icon_help.h \
        icon_open.h \
        main.c \
        man.c \
        man.h \
        misc.c \
        ScrollByL.c \
        ScrollByL.h \
        ScrollByLP.h \
        search.c \
        tkfuncs.c \
        vendor.c \
        vendor.h \
        version.h

if XAW_USE_XPRINT
xman_SOURCES += \
        print.c \
        print.h
endif

appman_PRE = \
        xman.man

# App default files  (*.ad)

appdefaultdir = @appdefaultdir@

if XAW_USE_XPRINT
Xman: Xman-xprint.ad
	ln -s $(top_srcdir)/Xman-xprint.ad Xman
else
Xman: Xman-noxprint.ad
	ln -s $(top_srcdir)/Xman-noxprint.ad Xman
endif

appdefault_DATA = Xman

EXTRA_DIST = Xman-xprint.ad Xman-noxprint.ad

CLEANFILES = Xman

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

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@
