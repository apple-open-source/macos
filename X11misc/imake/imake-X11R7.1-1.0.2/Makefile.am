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

CPP_PROGRAM = @CPP_PROGRAM@
XCONFDIR = @XCONFDIR@
PREPROCESSCMD_MKDEPEND = @PREPROCESSCMD_MKDEPEND@
ARCMD = @ARCMD@
RANLIB = @RANLIB@

if BUILD_REVPATH
REVPATH = revpath
endif

bin_PROGRAMS = imake $(REVPATH)

imake_CFLAGS = $(XPROTO_CFLAGS) -DCPP_PROGRAM='"$(CPP_PROGRAM)"'
imake_SOURCES = imake.c imakemdep.h

revpath_SOURCES = revpath.c

if BUILD_MAKEG
MAKEG = makeg
endif

if BUILD_XMKMF
XMKMF = xmkmf
endif

if BUILD_CCMAKEDEP
CCMAKEDEP = ccmakedep
endif

if BUILD_MERGELIB
MERGELIB = mergelib
endif

if BUILD_MKDIRHIER
MKDIRHIER = mkdirhier
endif

if BUILD_CLEANLINKS
CLEANLINKS = cleanlinks
endif

if BUILD_MKHTMLINDEX
MKHTMLINDEX = mkhtmlindex
endif

bin_SCRIPTS = \
	$(MAKEG) \
	$(XMKMF) \
	$(CCMAKEDEP) \
	$(MERGELIB) \
	$(MKDIRHIER) \
	$(CLEANLINKS) \
	$(MKHTMLINDEX)

xmkmf: xmkmf.cpp
	$(RAWCPP) $(RAWCPPFLAGS) -DCONFIGDIRSPEC='"-I$(XCONFDIR)"' < $(srcdir)/xmkmf.cpp | $(SED) -e /^\#/d | $(SED) -e s/XCOMM/\#/ > $@

ccmakedep: mdepend.cpp
	$(RAWCPP) $(RAWCPPFLAGS) -DPREPROC='"$(PREPROCESSCMD_MKDEPEND)"' < $(srcdir)/mdepend.cpp | $(SED) -e /^\#/d | $(SED) -e s/XCOMM/\#/ > $@

mergelib: mergelib.cpp
	$(RAWCPP) $(RAWCPPFLAGS) -DARCMD="$(ARCMD)" -DRANLIB="$(RANLIB)" < $(srcdir)/mergelib.cpp | $(SED) -e /^\#/d | $(SED) -e s/XCOMM/\#/ > $@

if HAS_PERL
MKHTMLINDEX_SCRIPT = $(srcdir)/mkhtmlindex.pl
else
MKHTMLINDEX_SCRIPT = $(srcdir)/mkhtmlindex.sh
endif

mkhtmlindex: $(MKHTMLINDEX_SCRIPT)
	cp $(MKHTMLINDEX_SCRIPT) $@


CLEANFILES = \
	xmkmf \
	ccmakedep \
	mergelib \
	mkhtmlindex

EXTRA_DIST = \
	makeg \
	xmkmf.cpp \
	mdepend.cpp \
	mergelib.cpp \
	mkdirhier \
	cleanlinks \
	mkhtmlindex.pl \
	mkhtmlindex.sh

# Man page
appmandir = $(APP_MAN_DIR)

appman_PRE = \
	imake.man \
	makeg.man \
	xmkmf.man \
	ccmakedep.man \
	mergelib.man \
	revpath.man \
	mkdirhier.man \
	cleanlinks.man \
	mkhtmlindex.man

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
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g' \
	-e 's|__cpp__|$(RAWCPP)|g'

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@
