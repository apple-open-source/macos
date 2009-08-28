EXTRAS = $(DSTROOT)/System/Library/Perl/Extras
ARCHLIB = $(shell perl -MConfig -e 'print $$Config::Config{archname}')

installsrc:
	pax -rw . $(SRCROOT)

clean:

installhdrs:

# multi-version support
VERSIONERDIR := /usr/local/versioner

# Perl multi-version support
PERLVERSIONS := $(VERSIONERDIR)/perl/versions
PERLSUBDEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERLVERSIONS))
PERLDEFAULT := $(shell grep '^$(PERLSUBDEFAULT)' $(PERLVERSIONS))
PERLUNORDEREDVERS := $(shell grep -v '^DEFAULT' $(PERLVERSIONS))
# do default version last
PERLORDEREDVERS := $(filter-out $(PERLDEFAULT),$(PERLUNORDEREDVERS)) $(PERLDEFAULT)

install:
	@set -x && \
	for vers in $(PERLORDEREDVERS); do \
	    export VERSIONER_PERL_VERSION=$${vers} && \
	    EXTRASPERL=$(EXTRAS)/$${vers} && \
	    mkdir -p $(OBJROOT)/$${vers} && \
	    make -C modules install EXTRAS=$(EXTRAS) EXTRASPERL=$${EXTRASPERL} \
		ARCHLIB=$(ARCHLIB) OBJROOT=$(OBJROOT)/$${vers} || exit 1; \
	done
	mkdir $(DSTROOT)/usr/bin
	ln $(EXTRAS)/bin/svk $(DSTROOT)/usr/bin/svk
	mkdir -p $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 svk.plist $(DSTROOT)/usr/local/OpenSourceVersions
	mkdir -p $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 0444 svk.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	find "$(EXTRAS)" -name '*.bs' -empty -delete
