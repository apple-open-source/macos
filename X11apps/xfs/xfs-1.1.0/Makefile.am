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

AM_CFLAGS = $(CWARNFLAGS)

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

appman_PRE = \
        xfs.man

FSERRORS = /var/log/xfs.log

FONTDIR = $(libdir)/X11/fonts

# Strings to replace in config.cpp
CPP_FILES_FLAGS = $(MANDEFS) \
	-DDEFAULTFONTPATH="$(FONTPATH)" -DFSERRORS="$(FSERRORS)"

config_DATA = config

EXTRA_DIST = xfs.def config.cpp
CLEANFILES = config

appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST += $(appman_PRE)
CLEANFILES += $(appman_DATA)

SED = sed

# Translate XCOMM into pound sign with sed, rather than passing -DXCOMM=XCOMM
# to cpp, because that trick does not work on all ANSI C preprocessors.
# Delete line numbers from the cpp output (-P is not portable, I guess).
# Allow XCOMM to be preceded by whitespace and provide a means of generating
# output lines with trailing backslashes.
# Allow XHASH to always be substituted, even in cases where XCOMM isn't.

CPP_SED_MAGIC = $(SED) -e '/^\#  *[0-9][0-9]*  *.*$$/d' \
                       -e '/^\#line  *[0-9][0-9]*  *.*$$/d' \
                       -e '/^[ 	]*XCOMM$$/s/XCOMM/\#/' \
                       -e '/^[ 	]*XCOMM[^a-zA-Z0-9_]/s/XCOMM/\#/' \
                       -e '/^[ 	]*XHASH/s/XHASH/\#/' \
                       -e '/XSLASHGLOB/s/XSLASHGLOB/\/\*/' \
                       -e '/\@\@$$/s/\@\@$$/\\/'

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MANDEFS =  \
	-D__xorgversion__="\"$(XORGRELSTRING)\" \"$(XORGMANNAME)\"" \
	-D__appmansuffix__=$(APP_MAN_SUFFIX) \
	-D__libmansuffix__=$(LIB_MAN_SUFFIX) \
	-D__adminmansuffix__=$(ADMIN_MAN_SUFFIX) \
	-D__miscmansuffix__=$(MISC_MAN_SUFFIX) \
	-D__filemansuffix__=$(FILE_MAN_SUFFIX) \
	-D__configfiledesc__="$(CONFIG_FILE_DESC)" \
	-D__configfilepath__="$(CONFIG_FILE_PATH)"

SUFFIXES = .$(APP_MAN_SUFFIX) .man .cpp

.cpp:
	$(RAWCPP) $(RAWCPPFLAGS) $(CPP_FILES_FLAGS) < $< | $(CPP_SED_MAGIC) > $@

.man.$(APP_MAN_SUFFIX):
	$(RAWCPP) $(RAWCPPFLAGS) $(MANDEFS) $(EXTRAMANDEFS) < $< | $(CPP_SED_MAGIC) > $@

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	       $(AM_CPPFLAGS) $(CPPFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(xfs_SOURCES)
endif LINT

# Developer documentation - not installed on system
EXTRA_DIST += doc/xfs-design.xml doc/xfs-design.css doc/xfs-design.xsl

if MAKE_DEVEL_DOCS
if HAVE_XMLTO
noinst_DATA = doc/xfs-design/index.html doc/xfs-design.pdf doc/xfs-design.txt
CLEANFILES += $(noinst_DATA)

doc/xfs-design/index.html: $(srcdir)/doc/xfs-design.xml $(srcdir)/doc/xfs-design.xsl doc/xfs-design/xfs-design.css
	$(XMLTO) -m $(srcdir)/doc/xfs-design.xsl -o doc/xfs-design xhtml $(srcdir)/doc/xfs-design.xml

doc/xfs-design/xfs-design.css: $(srcdir)/doc/xfs-design.css
	-rm -f doc/xfs-design/xfs-design.css
	$(MKDIR_P) doc/xfs-design
	cp -pf $(srcdir)/doc/xfs-design.css doc/xfs-design/xfs-design.css

clean-local:
	-rm -rf doc/xfs-design

doc/xfs-design.pdf: $(srcdir)/doc/xfs-design.xml $(srcdir)/doc/xfs-design.xsl
	$(XMLTO) -m $(srcdir)/doc/xfs-design.xsl -o doc pdf $(srcdir)/doc/xfs-design.xml

doc/xfs-design.txt: $(srcdir)/doc/xfs-design.xml $(srcdir)/doc/xfs-design.xsl
	$(XMLTO) -m $(srcdir)/doc/xfs-design.xsl -o doc txt $(srcdir)/doc/xfs-design.xml

endif HAVE_XMLTO
endif MAKE_DEVEL_DOCS
