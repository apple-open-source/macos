##
# Makefile for headerdoc
# Wilfredo Sanchez | wsanchez@apple.com
##

DSTROOT = /tmp/headerdoc/Release

bindir  = /usr/bin
docsDir = /Developer/Documentation/DeveloperTools
program1 = headerdoc2html
program2 = gatherheaderdoc

perl_libdir := $(shell perl -e 'require Config; print "$$Config::Config{'privlib'}\n";')
startperl   := $(shell perl -e 'require Config; print "$$Config::Config{'startperl'}\n";')

all:

installsrc:
	mkdir -p "$(SRCROOT)"
	tar cf - . | (cd "$(SRCROOT)" && tar xpf -)

installhdrs:

build:

clean:

install:
	umask 022 && install -d $(DSTROOT)$(perl_libdir)/HeaderDoc
	install -c -m 444 Modules/HeaderDoc/*.pm $(DSTROOT)$(perl_libdir)/HeaderDoc
	umask 022 && install -d $(DSTROOT)$(bindir)
	install -c -m 755 headerDoc2HTML.pl $(DSTROOT)$(bindir)/$(program1)
	perl -i -pe 's|^#!/usr/bin/perl.*$$|$(startperl)|;' $(DSTROOT)$(bindir)/$(program1)
	chmod 555 $(DSTROOT)$(bindir)/$(program1)
	umask 022 && install -d $(DSTROOT)$(bindir)
	install -c -m 755 gatherHeaderDoc.pl $(DSTROOT)$(bindir)/$(program2)
	perl -i -pe 's|^#!/usr/bin/perl.*$$|$(startperl)|;' $(DSTROOT)$(bindir)/$(program2)
	chmod 555 $(DSTROOT)$(bindir)/$(program2)
	umask 022 && install -d $(DSTROOT)$(docsDir)/HeaderDoc
	install -c -m 444 Documentation/*.html $(DSTROOT)$(docsDir)/HeaderDoc
