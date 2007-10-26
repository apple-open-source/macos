##
# Makefile for headerdoc
# Wilfredo Sanchez | wsanchez@apple.com
##


bindir  = /usr/bin
confdir = /Library/Preferences/
conffile = com.apple.headerDoc2HTML.config
templatefile = com.apple.headerdoc.exampletocteplate.html
# docsDir = /Developer/Documentation/DeveloperTools
program1 = headerdoc2html
program2 = gatherheaderdoc

os := $(shell uname -s)
osmajor := $(shell uname -r | sed 's/\..*//')
perl_libdir := $(shell perl -e 'require Config; print "$$Config::Config{'privlib'}\n";')
ifeq ($(os),Darwin)
ifeq ($(shell test $(osmajor) -ge 8 && echo yes),yes)
perl_libdir := $(subst Perl,Perl/Extras,$(perl_libdir))
endif
endif
startperl   := $(shell perl -e 'require Config; print "$$Config::Config{'startperl'}\n";')

all:
	cd xmlman ; make all ARCH=`uname` VERS=`sw_vers -productVersion`

clean:
	cd xmlman ; make clean

installsrc:
	mkdir -p "$(SRCROOT)"
	tar cf - . | (cd "$(SRCROOT)" && tar xpf -)

installhdrs:

build:

clean:

test:
	cd testsuite ; make ; make runtests ; cd ..

realinstall: all
	DSTROOT="" make installsub

install: all
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
	install -c -m 444 Modules/HeaderDoc/Availability.list $(DSTROOT)$(perl_libdir)/HeaderDoc
	if [ -f "Modules/HeaderDoc/LinkResolver.pm" ] ; then \
		rm -f $(DSTROOT)$(perl_libdir)/HeaderDoc/LinkResolver.pm ; \
		umask 022 && install -d $(DSTROOT)/AppleInternal/Library/Perl/HeaderDoc ; \
		install -c -m 444 Modules/HeaderDoc/LinkResolver.pm $(DSTROOT)/AppleInternal/Library/Perl/HeaderDoc ; \
	fi
	umask 022 && install -d $(DSTROOT)$(bindir)
	umask 022 && install -d $(DSTROOT)$(perl_libdir)/HeaderDoc/bin
	umask 022 && install -d $(DSTROOT)$(confdir)
	install -c -m 755 headerDoc2HTML.config-xcodecolors $(DSTROOT)$(confdir)$(conffile)
	install -c -m 444 exampletoctemplate.html $(DSTROOT)$(confdir)$(templatefile)
	install -c -m 755 xmlman/xml2man $(DSTROOT)$(bindir)/xml2man
	install -c -m 755 xmlman/hdxml2manxml $(DSTROOT)$(bindir)/hdxml2manxml
	install -c -m 755 xmlman/resolveLinks $(DSTROOT)$(perl_libdir)/HeaderDoc//bin/resolveLinks
	install -c -m 755 headerDoc2HTML.pl $(DSTROOT)$(bindir)/$(program1)
	perl -i -pe 's|^#!/usr/bin/perl.*$$|$(startperl)|;' $(DSTROOT)$(bindir)/$(program1)
	chmod 555 $(DSTROOT)$(bindir)/$(program1)
	umask 022 && install -d $(DSTROOT)$(bindir)
	install -c -m 755 gatherHeaderDoc.pl $(DSTROOT)$(bindir)/$(program2)
	perl -i -pe 's|^#!/usr/bin/perl.*$$|$(startperl)|;' $(DSTROOT)$(bindir)/$(program2)
	chmod 555 $(DSTROOT)$(bindir)/$(program2)
	# umask 022 && install -d $(DSTROOT)$(docsDir)/HeaderDoc
	# install -c -m 444 Documentation/*.html $(DSTROOT)$(docsDir)/HeaderDoc
	umask 022 && install -d $(DSTROOT)/usr/share/man/man1
	install -c -m 444 Documentation/man/*.1 $(DSTROOT)/usr/share/man/man1
	umask 022 && install -d $(DSTROOT)/usr/share/man/man5
	install -c -m 444 Documentation/man/*.5 $(DSTROOT)/usr/share/man/man5
	cd xmlman ; make clean ; cd ..

