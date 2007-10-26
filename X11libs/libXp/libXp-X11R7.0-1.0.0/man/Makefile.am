# $Id: Makefile.am,v 1.6 2005/12/06 22:48:44 kem Exp $
#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# 
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the name of the copyright holders shall
# not be used in advertising or otherwise to promote the sale, use or
# other dealings in this Software without prior written authorization
# from the copyright holders.
# 

libmandir = $(LIB_MAN_DIR)

libman_PRE = \
	libXp.man				\
	XpCancelDoc.man				\
	XpCancelJob.man				\
	XpCancelPage.man			\
	XpCreateContext.man			\
	XpDestroyContext.man			\
	XpEndDoc.man				\
	XpEndJob.man				\
	XpEndPage.man				\
	XpFreePrinterList.man			\
	XpGetAttributes.man			\
	XpGetContext.man			\
	XpGetDocumentData.man			\
	XpGetImageResolution.man		\
	XpGetLocaleHinter.man			\
	XpGetOneAttribute.man			\
	XpGetPageDimensions.man			\
	XpGetPdmStartParams.man			\
	XpGetPrinterList.man			\
	XpGetScreenOfContext.man		\
	XpInputSelected.man			\
	XpPutDocumentData.man			\
	XpQueryExtension.man			\
	XpQueryScreens.man			\
	XpQueryVersion.man			\
	XpRehashPrinterList.man			\
	XpSelectInput.man			\
	XpSetAttributes.man			\
	XpSetContext.man			\
	XpSetImageResolution.man		\
	XpSetLocaleHinter.man			\
	XpStartDoc.man				\
	XpStartJob.man				\
	XpStartPage.man


EXTRA_DIST = $(libman_PRE)

CLEANFILES = $(libman_DATA)

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = -e 's/__xorgversion__/"$(XORGRELSTRING)" "$(XORGMANNAME)"/'

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

.man.$(LIB_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@

