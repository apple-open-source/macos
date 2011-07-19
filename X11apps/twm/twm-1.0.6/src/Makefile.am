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
#

AM_YFLAGS = -d

bin_PROGRAMS = twm

rcdir = ${datadir}/X11/twm
dist_rc_DATA = system.twmrc

AM_CFLAGS = $(TWM_CFLAGS) -DXVENDORNAME=\"The\ X.Org\ Foundation\" -DXORG_RELEASE=\"Release\ $(VERSION)\" -D_BSD_SOURCE
AM_CFLAGS += -DSYSTEM_INIT_FILE=\"${datadir}/X11/twm/system.twmrc\"

twm_LDADD = $(TWM_LIBS)

twm_SOURCES =   \
        add_window.c \
        add_window.h \
        cursor.c \
	deftwmrc.c \
        events.c \
        events.h \
        gc.c \
        gc.h \
        iconmgr.c \
        iconmgr.h \
        icons.c \
        icons.h \
        list.c \
        list.h \
        menus.c \
        menus.h \
        parse.c \
        parse.h \
        resize.c \
        resize.h \
        screen.h \
        session.c \
        session.h \
        twm.c \
        twm.h \
        util.c \
        util.h \
        version.c \
        version.h \
	gram.y \
	lex.l

BUILT_SOURCES = gram.h gram.c lex.c deftwmrc.c
MAINTAINERCLEANFILES = $(BUILT_SOURCES)

gram.h: gram.c

deftwmrc.c:  system.twmrc
	$(AM_V_GEN)rm -f $@ ; \
	echo '/* ' >>$@ ; \
	echo ' * This file is generated automatically from the default' >>$@ ; \
	echo ' * twm bindings file system.twmrc by the twm Makefile.' >>$@ ; \
	echo ' */' >>$@ ; \
	echo '' >>$@ ; \
	echo 'unsigned char *defTwmrc[] = {' >>$@ ; \
	sed -f ${srcdir}/deftwmrc.sed < ${srcdir}/system.twmrc >>$@ ; \
	echo '    (unsigned char *) 0 };' >>$@

EXTRA_DIST = deftwmrc.sed siconify.bm
