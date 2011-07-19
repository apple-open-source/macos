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

AM_CFLAGS = $(CWARNFLAGS) $(XMH_CFLAGS) -D_BSD_SOURCE

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
        viewfuncs.c \
        xmh.h

appman_PRE = \
        xmh.man

# App default files
DISTCHECK_CONFIGURE_FLAGS = --with-appdefaultdir=\$${datadir}/X11/app-defaults

appdefaultdir = @appdefaultdir@

dist_appdefault_DATA = \
        app-defaults/Xmh

#

EXTRA_DIST = Xmh.sample


appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST += $(appman_PRE)
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
