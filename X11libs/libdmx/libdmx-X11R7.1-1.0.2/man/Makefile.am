#  Copyright 2005  Red Hat, Inc.
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Red Hat makes no representations about
#  the suitability of this software for any purpose.  It is provided "as is" without
#  express or implied warranty.
# 
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.
#

libmandir = $(LIB_MAN_DIR)

libman_PRE = \
         DMX.man \
         DMXAddInput.man \
         DMXAddScreen.man \
         DMXChangeDesktopAttributes.man \
         DMXChangeScreensAttributes.man \
         DMXForceWindowCreation.man \
         DMXGetDesktopAttributes.man \
         DMXGetInputAttributes.man \
         DMXGetInputCount.man \
         DMXGetScreenAttributes.man \
         DMXGetScreenCount.man \
         DMXGetWindowAttributes.man \
         DMXQueryExtension.man \
         DMXQueryVersion.man \
         DMXRemoveInput.man \
         DMXRemoveScreen.man \
         DMXSync.man

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

EXTRA_DIST = $(libman_PRE)

CLEANFILES = $(libman_DATA)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = \
	-e 's/__vendorversion__/"$(XORGRELSTRING)" "$(XORGMANNAME)"/' \
	-e 's/__libmansuffix__/$(LIB_MAN_SUFFIX)/g' 

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

.man.$(LIB_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@
