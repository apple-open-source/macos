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

bin_PROGRAMS = x11perf

bin_SCRIPTS = x11perfcomp

LIBPATH = $(libdir)/X11/x11perfcomp

x11perfcompdir = $(LIBPATH)
dist_x11perfcomp_SCRIPTS = fillblnk perfboth perfratio Xmark

AM_CFLAGS = $(XEXT_CFLAGS) $(XFT_CFLAGS) $(XRENDER_CFLAGS) $(X11PERF_CFLAGS)
x11perf_LDADD = $(XEXT_LIBS) $(XFT_LIBS) $(XRENDER_LIBS) $(X11PERF_LIBS) -lm

x11perf_SOURCES =	\
        bitmaps.c \
        bitmaps.h \
        do_arcs.c \
        do_blt.c \
        do_complex.c \
        do_dots.c \
        do_lines.c \
        do_movewin.c \
        do_rects.c \
        do_segs.c \
        do_simple.c \
        do_tests.c \
        do_text.c \
        do_traps.c \
        do_tris.c \
        do_valgc.c \
        do_windows.c \
        x11perf.c \
        x11perf.h

appman_PRE = \
        x11perfcomp.man \
        x11perf.man \
        Xmark.man

x11perfcomp: x11pcomp.cpp
	$(SED) s/LIBPATH/`echo $(LIBPATH) | sed -e s/\\\\//\\\\\\\\\\\\\//g`/ < $(srcdir)/x11pcomp.cpp | \
		$(SED) s/XCOMM/\#/ > $@


CLEANFILES = $(bin_SCRIPTS) x11perfcomp

EXTRA_DIST = x11pcomp.cpp

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
