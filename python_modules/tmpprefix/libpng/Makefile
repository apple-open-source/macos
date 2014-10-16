# libpng
#
# Build libpng into OBJROOT to link into matplotlib.
# Installation directory is given by the variable TMPPREFIX.
# Set RC_CFLAGS to build multiple architectures.

PNGNAME = libpng
PNGVERSORIG = 1.6.2
PNGVERS = $(PNGVERSORIG)-nocontrib
PNGNAMEVERS = $(PNGNAME)-$(PNGVERS)
PNGNAMEVERSORIG = $(PNGNAME)-$(PNGVERSORIG)

PNGSRC = $(SRCROOT)/tmpprefix/$(PNGNAME)
PNGTOP = $(OBJROOT)/$(PNGNAME)
PNGDIR = $(PNGTOP)/$(PNGNAMEVERS)

install: $(PNGDIR) $(PNGDIR)/darwin

$(PNGDIR):
	mkdir -p $(PNGTOP)
	tar xzof $(PNGNAMEVERS).tar.gz -C $(PNGTOP)
	cp -f $(PNGDIR)/LICENSE $(OSL)/$(PNGNAMEVERSORIG).txt
	cp -f $(PNGSRC)/$(PNGNAME).partial $(OSV)/$(PNGNAMEVERSORIG).partial

$(PNGDIR)/darwin:
	@set -x && \
	cd $(PNGDIR) && \
	mkdir darwin && \
	cd darwin && \
	CFLAGS='$(RC_CFLAGS)' LDFLAGS='$(RC_CFLAGS)' ../configure --disable-dependency-tracking --disable-shared --enable-static --prefix=$(TMPPREFIX) && \
	make && \
	make install

afterinstallsrc:
	gnutar xozf $(PNGNAMEVERSORIG).tar.gz
	rm -rf $(PNGNAMEVERSORIG)/contrib
	mv $(PNGNAMEVERSORIG) $(PNGNAMEVERS)
	gnutar czf $(PNGNAMEVERS).tar.gz $(PNGNAMEVERS)
	rm -f $(PNGNAMEVERSORIG).tar.gz
	rm -rf $(PNGNAMEVERS)
