#  Copyright 2005 Red Hat, Inc.
#
#  Permission to use, copy, modify, distribute, and sell this software
#  and its documentation for any purpose is hereby granted without
#  fee, provided that the above copyright notice appear in all copies
#  and that both that copyright notice and this permission notice
#  appear in supporting documentation, and that the name of Red Hat
#  not be used in advertising or publicity pertaining to distribution
#  of the software without specific, written prior permission.  Red
#  Hat makes no representations about the suitability of this software
#  for any purpose.  It is provided "as is" without express or implied
#  warranty.
#
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
#  NO EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
#  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
#  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
#  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

FONT_FILES = \
	c0419bt_.afm \
	c0419bt_.pfb \
	c0582bt_.afm \
	c0582bt_.pfb \
	c0583bt_.afm \
	c0583bt_.pfb \
	c0611bt_.afm \
	c0611bt_.pfb \
	c0632bt_.afm \
	c0632bt_.pfb \
	c0633bt_.afm \
	c0633bt_.pfb \
	c0648bt_.afm \
	c0648bt_.pfb \
	c0649bt_.afm \
	c0649bt_.pfb

fontdir = @FONTDIR@
font_DATA = $(FONT_FILES)

EXTRA_DIST = $(FONT_FILES) Copyright autogen.sh

install-data-hook:
	@rm -f $(DESTDIR)$(fontdir)/fonts.scale
	$(MKFONTSCALE) $(DESTDIR)$(fontdir)
	@rm -f $(DESTDIR)$(fontdir)/font.dir
	$(MKFONTDIR) $(DESTDIR)$(fontdir)
	$(FCCACHE) $(DESTDIR)$(fontdir)

distuninstallcheck:
	@:
