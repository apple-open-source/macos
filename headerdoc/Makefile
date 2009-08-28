##
# Makefile for headerdoc
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

all: all_internal test

all_internal:
	cd xmlman ; make all ARCH=`uname` VERS=`sw_vers -productVersion | sed 's/\([0-9][0-9]*\)\.\([0-9][0-9]*\)\..*/\1.\2/'` ; cd ..

clean:
	cd xmlman ; make clean ; cd ..

installsrc:
	mkdir -p "$(SRCROOT)"
	tar cf - . | (cd "$(SRCROOT)" && tar xpf -)

installhdrs:

build:

clean:

test:
	./headerDoc2HTML.pl -T run; \
	if [ "$$?" -ne 0 ] ; then \
		echo "Test suite failed."; \
		exit -1; \
	fi

	rm -rf /tmp/hdtest_perm
	rm -rf /tmp/hdtest_out
	mkdir /tmp/hdtest_perm
	cp ExampleHeaders/template.h /tmp/hdtest_perm
	cp ExampleHeaders/textblock.h /tmp/hdtest_perm
	cp ExampleHeaders/throwtest.h /tmp/hdtest_perm
	cp ExampleHeaders/typedefTest.h /tmp/hdtest_perm
	chmod u=w,og= /tmp/hdtest_perm/throwtest.h

	if [ "x$$USER" != "xroot" ] ; then \
		echo "Testing to make sure HeaderDoc returns an error if a file could not be read." ;\
		if ./headerDoc2HTML.pl -c headerDoc2HTML.config-installed -o /tmp/hdtest_out /tmp/hdtest_perm > /dev/null 2>/dev/null ; then \
			echo "Permission test failed."; \
			rm -rf /tmp/hdtest_perm; \
			rm -rf /tmp/hdtest_out; \
			exit -1; \
		fi \
	fi
	rm -rf /tmp/hdtest_perm
	rm -rf /tmp/hdtest_out
	exit 0
	# cd testsuite ; make ; make runtests ; cd ..

realinstall: all_internal
	DSTROOT="" make installsub

install: all_internal
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

