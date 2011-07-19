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

SUBDIRS=config doc man

bin_PROGRAMS = xfs

FONT_FLAGS = -DFONT_PCF -DFONT_FS -DFONT_SPEEDO

AM_CPPFLAGS = $(XFS_CFLAGS) -I$(top_srcdir)/include -DXFSPIDDIR=\"/var/run\" \
	$(FONT_FLAGS) -D_BSD_SOURCE -DFONT_t -DTRANS_SERVER -DTRANS_REOPEN

AM_CFLAGS = $(CWARNFLAGS)

ACLOCAL_AMFLAGS = -I m4

LDADD = $(XFS_LIBS)

xfs_SOURCES = \
	difs/atom.c \
	difs/cache.c \
	difs/charinfo.c \
	difs/difsutils.c \
	difs/dispatch.c \
	difs/events.c \
	difs/extensions.c \
	difs/fontinfo.c \
	difs/fonts.c \
	difs/globals.c \
	difs/initfonts.c \
	difs/main.c \
	difs/resource.c \
	difs/swaprep.c \
	difs/swapreq.c \
	difs/tables.c \
	os/access.c \
	os/config.c \
	os/config.h \
	os/configstr.h \
	os/connection.c \
	os/daemon.c \
	os/error.c \
	os/io.c \
	os/osdep.h \
	os/osglue.c \
	os/osinit.c \
	os/utils.c \
	os/waitfor.c \
	os/xfstrans.c \
	include/access.h \
	include/assert.h \
	include/auth.h \
	include/authstr.h \
	include/cache.h \
	include/cachestr.h \
	include/client.h \
	include/clientstr.h \
	include/closestr.h \
	include/closure.h \
	include/difsfn.h \
	include/difsfnst.h \
	include/difs.h \
	include/difsutils.h \
	include/dispatch.h \
	include/extentst.h \
	include/fsevents.h \
	include/fsresource.h \
	include/globals.h \
	include/misc.h \
	include/os.h \
	include/osstruct.h \
	include/servermd.h \
	include/site.h \
	include/swaprep.h \
	include/swapreq.h

EXTRA_DIST = xfs.def
MAINTAINERCLEANFILES = ChangeLog INSTALL

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	       $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(xfs_SOURCES)
endif LINT
