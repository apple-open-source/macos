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

AM_CFLAGS = $(CWARNFLAGS) $(XMAN_CFLAGS) \
	-D_BSD_SOURCE -DHELPFILE=\"$(helpdir)/xman.help\"
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
        vendor.h

appman_PRE = \
        xman.man

# App default files

DISTCHECK_CONFIGURE_FLAGS = --with-appdefaultdir=\$${datadir}/X11/app-defaults

appdefaultdir = @appdefaultdir@

dist_appdefault_DATA = app-defaults/Xman

appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST = $(appman_PRE)
MAINTAINERCLEANFILES = ChangeLog INSTALL
CLEANFILES = $(appman_DATA)

SUFFIXES = .$(APP_MAN_SUFFIX) .man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
.man.$(APP_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@


.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
