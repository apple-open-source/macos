##
# Makefile for headerdoc
# Wilfredo Sanchez | wsanchez@apple.com
##


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

realinstall:
	DSTROOT="" make installsub

install:
	@echo ; \
	export DSTROOT="/tmp/headerdoc/Release" ; \
 \
	echo "WARNING: Make install by default installs in" ; \
	echo "" ; \
	echo "          $$DSTROOT" ; \
	echo "" ; \
	echo "This is primarily intended for building packages." ; \
	echo "If you want to actually install over your" ; \
	echo "existing installation, cancel this make and run" ; \
	echo "\"sudo make realinstall\" instead." ; \
 \
	sleep 5 ; \
	make installsub

installsub:

	@echo "Destination is:  \"${DSTROOT}\""

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
	umask 022 && install -d $(DSTROOT)/usr/share/man/man1
	install -c -m 444 Documentation/man/*.1 $(DSTROOT)/usr/share/man/man1

