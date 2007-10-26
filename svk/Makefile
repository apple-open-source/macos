VERSION = $(shell perl -MConfig -e 'print $$Config::Config{version}')
EXTRAS = $(DSTROOT)/System/Library/Perl/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
ARCHLIB = $(shell perl -MConfig -e 'print $$Config::Config{archname}')

installsrc:
	pax -rw . $(SRCROOT)

clean:

installhdrs:

install:
	make -C modules install EXTRAS=$(EXTRAS) EXTRASPERL=$(EXTRASPERL) \
		ARCHLIB=$(ARCHLIB) OBJROOT=$(OBJROOT)
	mkdir $(DSTROOT)/usr/bin
	ln $(EXTRAS)/bin/svk $(DSTROOT)/usr/bin/svk
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 svk.plist $(DSTROOT)/usr/local/OpenSourceVersions
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 0444 svk.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	find "$(EXTRASPERL)" -name '*.bs' -empty -delete
