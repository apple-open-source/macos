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

configdir = $(sysconfdir)/X11/fs

bin_PROGRAMS = xfs

FONT_FLAGS = -DFONT_PCF -DFONT_FS -DFONT_SPEEDO

AM_CPPFLAGS = $(XFS_CFLAGS) -I$(top_srcdir)/include -DXFSPIDDIR=\"/var/run\" \
	$(FONT_FLAGS) -D_BSD_SOURCE -DFONT_t -DTRANS_SERVER -DTRANS_REOPEN

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
	include/accstr.h \
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

appman_PRE = \
        xfs.man

FSERRORS = /var/log/xfs.log

FONTDIR = $(libdir)/X11/fonts

FONTPATH = $(FONTDIR)/misc/,$(FONTDIR)/TTF/,$(FONTDIR)/Type1/,$(FONTDIR)/CID/,$(FONTDIR)/100dpi/,$(FONTDIR)/75dpi/

config: config.cpp
	sed -e s/XCOMM/\#/ -e s@DEFAULTFONTPATH@$(FONTPATH)@ -e s@FSERRORS@$(FSERRORS)@ < $(top_srcdir)/config.cpp > config

config_DATA = config

EXTRA_DIST = xfs.def config.cpp
CLEANFILES = config

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
	-e 's|__configdir__|$(configdir)|' \
	-e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
	-e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
	-e 's|__adminmansuffix__|$(ADMIN_MAN_SUFFIX)|g' \
	-e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g' \
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g' \
	-e 's|__configfiledesc__|$(CONFIG_FILE_DESC)|g' \
	-e 's|__configfilepath__|$(CONFIG_FILE_PATH)|g' \
	-e 's|XCOMM|\#|g'

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	       $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(xfs_SOURCES)
endif LINT
