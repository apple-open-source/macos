# libpng
#
# Build libpng into OBJROOT to link into matplotlib.
# Installation directory is given by the variable TMPPREFIX.
# Set RC_CFLAGS to build multiple architectures.

PNGNAME = libpng
PNGVERS = 1.6.36
PNGNAMEVERS = $(PNGNAME)-$(PNGVERS)

PNGSRC = $(SRCROOT)/tmpprefix/$(PNGNAME)
PNGTOP = $(OBJROOT)/$(PNGNAME)
PNGDIR = $(PNGTOP)/$(PNGNAMEVERS)

install: $(PNGDIR) $(PNGDIR)/darwin

$(PNGDIR):
	mkdir -p $(PNGTOP)
	cp -a $(PNGSRC)/$(PNGNAMEVERS) $(PNGTOP)
	cp -f $(PNGDIR)/LICENSE $(OSL)/$(PNGNAMEVERS).txt
	cp -f $(PNGSRC)/$(PNGNAME).partial $(OSV)/$(PNGNAMEVERS).partial

$(PNGDIR)/darwin:
	@set -x && \
	cd $(PNGDIR) && \
	mkdir darwin && \
	cd darwin && \
	CFLAGS='$(RC_CFLAGS)' LDFLAGS='$(RC_CFLAGS)' ../configure --disable-dependency-tracking --disable-shared --enable-static --prefix=$(TMPPREFIX) --enable-arm-neon && \
	sed -e '/PNG_ARM_NEON_OPT/ s/^/\/\//' -i '.orig' config.h && \
	make libpng16.la && \
	make install-nodist_pkgincludeHEADERS pkgincludedir="$(TMPPREFIX)/include" && \
	make install-pkgincludeHEADERS pkgincludedir="$(TMPPREFIX)/include" && \
	make install-libLTLIBRARIES && \
	cp "$(TMPPREFIX)/lib/libpng16.a" "$(TMPPREFIX)/lib/libpng.a"
