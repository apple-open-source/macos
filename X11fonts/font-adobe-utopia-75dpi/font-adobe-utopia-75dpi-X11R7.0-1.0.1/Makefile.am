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

BDF2UCS_FONT_FILES = \
	UTB___10 \
	UTB___12 \
	UTB___14 \
	UTB___18 \
	UTB___24 \
	UTBI__10 \
	UTBI__12 \
	UTBI__14 \
	UTBI__18 \
	UTBI__24 \
	UTI___10 \
	UTI___12 \
	UTI___14 \
	UTI___18 \
	UTI___24 \
	UTRG__10 \
	UTRG__12 \
	UTRG__14 \
	UTRG__18 \
	UTRG__24
FONT_FILES = $(BDF2UCS_FONT_FILES)

BDF_FILES = $(FONT_FILES:%=%.bdf)
PCF_FILES = $(FONT_FILES:%=%.pcf.gz)

if ISO8859_1
ISO8859_1_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-1.bdf)
ISO8859_1_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-1.pcf.gz)
endif

if ISO8859_2
ISO8859_2_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-2.bdf)
ISO8859_2_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-2.pcf.gz)
endif

if ISO8859_3
ISO8859_3_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-3.bdf)
ISO8859_3_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-3.pcf.gz)
endif

if ISO8859_4
ISO8859_4_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-4.bdf)
ISO8859_4_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-4.pcf.gz)
endif

if ISO8859_9
ISO8859_9_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-9.bdf)
ISO8859_9_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-9.pcf.gz)
endif

if ISO8859_10
ISO8859_10_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-10.bdf)
ISO8859_10_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-10.pcf.gz)
endif

if ISO8859_13
ISO8859_13_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-13.bdf)
ISO8859_13_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-13.pcf.gz)
endif

if ISO8859_14
ISO8859_14_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-14.bdf)
ISO8859_14_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-14.pcf.gz)
endif

if ISO8859_15
ISO8859_15_BDF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-15.bdf)
ISO8859_15_PCF_FILES = $(BDF2UCS_FONT_FILES:%=%-ISO8859-15.pcf.gz)
endif

UTIL_DIR = @MAPFILES_PATH@

SUFFIXES = .bdf .pcf.gz \
	-ISO8859-1.bdf \
	-ISO8859-2.bdf \
	-ISO8859-3.bdf \
	-ISO8859-4.bdf \
	-ISO8859-9.bdf \
	-ISO8859-10.bdf \
	-ISO8859-13.bdf \
	-ISO8859-14.bdf \
	-ISO8859-15.bdf

.bdf.pcf.gz:
	@rm -f $@
	$(BDFTOPCF) -t $< | gzip > $@

.bdf-ISO8859-1.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-1 ISO8859-1

.bdf-ISO8859-2.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-2 ISO8859-2

.bdf-ISO8859-3.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-3 ISO8859-3

.bdf-ISO8859-4.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-4 ISO8859-4

.bdf-ISO8859-9.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-9 ISO8859-9

.bdf-ISO8859-10.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-10 ISO8859-10

.bdf-ISO8859-13.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-13 ISO8859-13

.bdf-ISO8859-14.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-14 ISO8859-14

.bdf-ISO8859-15.bdf:
	@rm -f $@
	$(UCS2ANY) $< $(UTIL_DIR)/map-ISO8859-15 ISO8859-15

fontdir = @FONTDIR@
font_DATA = \
	$(PCF_FILES) \
	$(ISO8859_1_PCF_FILES) \
	$(ISO8859_2_PCF_FILES) \
	$(ISO8859_3_PCF_FILES) \
	$(ISO8859_4_PCF_FILES) \
	$(ISO8859_9_PCF_FILES) \
	$(ISO8859_10_PCF_FILES) \
	$(ISO8859_13_PCF_FILES) \
	$(ISO8859_14_PCF_FILES) \
	$(ISO8859_15_PCF_FILES)
BUILT_BDF_FILES = \
	$(ISO8859_1_BDF_FILES) \
	$(ISO8859_2_BDF_FILES) \
	$(ISO8859_3_BDF_FILES) \
	$(ISO8859_4_BDF_FILES) \
	$(ISO8859_9_BDF_FILES) \
	$(ISO8859_10_BDF_FILES) \
	$(ISO8859_13_BDF_FILES) \
	$(ISO8859_14_BDF_FILES) \
	$(ISO8859_15_BDF_FILES)
CLEANFILES = $(font_DATA) $(BUILT_BDF_FILES)

EXTRA_DIST = $(BDF_FILES) autogen.sh

all-local: $(BUILT_BDF_FILES) $(font_DATA)

install-data-hook:
	@rm -f $(DESTDIR)$(fontdir)/fonts.scale
	$(MKFONTSCALE) $(DESTDIR)$(fontdir)
	@rm -f $(DESTDIR)$(fontdir)/font.dir
	$(MKFONTDIR) $(DESTDIR)$(fontdir)

distuninstallcheck:
	@:
