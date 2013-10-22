# freetype
#
# Build freetype into OBJROOT and let matplotlib use it to build.
# Installation directory is given by the variable TMPPREFIX.
# Set RC_CFLAGS to build multiple architectures.

FREETYPENAME = freetype
FREETYPEVERS = 2.4.10
FREETYPENAMEVERS = $(FREETYPENAME)-$(FREETYPEVERS)

FREETYPESRC = $(SRCROOT)/tmpprefix/$(FREETYPENAME)
FREETYPETOP = $(OBJROOT)/$(FREETYPENAME)
FREETYPEDIR = $(FREETYPETOP)/$(FREETYPENAMEVERS)

install: $(FREETYPEDIR) $(FREETYPEDIR)/darwin

$(FREETYPEDIR):
	mkdir -p $(FREETYPETOP)
	tar xjof $(FREETYPENAMEVERS).tar.bz2 -C $(FREETYPETOP)
	cp $(FREETYPEDIR)/builds/mac/ftmac.c $(FREETYPEDIR)/src/base/ftmac.c
	ed - $(FREETYPEDIR)/include/freetype/config/ftoption.h < $(FREETYPESRC)/ftoption.h.ed
	cp -f $(FREETYPEDIR)/docs/LICENSE.TXT $(OSL)/$(FREETYPENAMEVERS).txt
	cp -f $(FREETYPESRC)/$(FREETYPENAME).partial $(OSV)/$(FREETYPENAMEVERS).partial

$(FREETYPEDIR)/darwin:
	@set -x && \
	cd $(FREETYPEDIR) && \
	mkdir darwin && \
	cd darwin && \
	CFLAGS='$(RC_CFLAGS)' LDFLAGS='$(RC_CFLAGS)' ../configure --without-fsspec --without-fsref --without-quickdraw-toolbox --disable-shared --enable-static --prefix=$(TMPPREFIX) && \
	make && \
	make install

afterinstallsrc:
	@true
