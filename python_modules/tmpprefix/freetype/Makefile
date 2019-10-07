# freetype
#
# Build freetype into OBJROOT and let matplotlib use it to build.
# Installation directory is given by the variable TMPPREFIX.
# Set RC_CFLAGS to build multiple architectures.

FREETYPENAME = freetype
FREETYPEVERS = 2.10.0
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

# We create the ft2build.h symlink because newer freetype versions moved it to
# the new location, but our old matplotlib doesn't know to look at the new
# location.  We can remove it if we update matplotlib.
$(FREETYPEDIR)/darwin:
	@set -x && \
	cd $(FREETYPEDIR) && \
	mkdir darwin && \
	cd darwin && \
	CFLAGS='$(RC_CFLAGS)' LDFLAGS='$(RC_CFLAGS)' ../configure --without-fsspec --without-fsref --without-quickdraw-toolbox --disable-shared --enable-static --enable-freetype-config --prefix=$(TMPPREFIX) && \
	make && \
	make install
	ed - $(TMPPREFIX)/bin/freetype-config < $(FREETYPESRC)/bin_freetype-config.ed
	ln -s freetype2/ft2build.h $(TMPPREFIX)/include

afterinstallsrc:
	@true
