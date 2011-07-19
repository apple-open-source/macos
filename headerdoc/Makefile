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

building_ppc = $(shell echo "$$RC_ARCHS" | grep -c ppc)

os := $(shell uname -s)
osmajor := $(shell uname -r | sed 's/\..*//')
perl_libdir := $(shell perl -e 'require Config; print "$$Config::Config{'privlib'}\n";')
ifeq ($(os),Darwin)
ifeq ($(shell test $(osmajor) -ge 8 && echo yes),yes)
perl_libdir := $(subst Perl,Perl/Extras,$(perl_libdir))
endif
endif
startperl   := $(shell perl -e 'require Config; print "$$Config::Config{'startperl'}\n";')

all: all_internal test apidoc

# Override the default compiler to GCC 4.0 if building for Snow Leopard internally.
all_internal:
	cd xmlman ; make all CC=`if [ "$$DEVELOPER_BIN_DIR" != "" -a "$(building_ppc)" != "0" ] ; then echo "gcc-4.0" ; else echo "cc"; fi` ARCH=`uname` VERS=`sw_vers -productVersion | sed 's/\([0-9][0-9]*\)\.\([0-9][0-9]*\)\..*/\1.\2/'` ; cd ..

clean:
	cd xmlman ; make clean ; cd ..
	rm -rf Documentation/hdapi

installsrc:
	mkdir -p "$(SRCROOT)"
	tar cf - . | (cd "$(SRCROOT)" && tar xpf -)

installhdrs:

build:

test:
	./headerDoc2HTML.pl -T run; \
	if [ "$$?" -ne 0 ] ; then \
		echo "Test suite failed."; \
		exit 1; \
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
			exit 1; \
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
	if [ "$(SYMROOT)" != "" ] ; then \
		umask 022 && install -d $(SYMROOT)$(bindir); \
	fi
	umask 022 && install -d $(DSTROOT)$(bindir)
	umask 022 && install -d $(DSTROOT)$(confdir)
	install -c -m 755 headerDoc2HTML.config-xcodecolors $(DSTROOT)$(confdir)$(conffile)
	install -c -m 444 exampletoctemplate.html $(DSTROOT)$(confdir)$(templatefile)
	if [ "$(SYMROOT)" != "" ] ; then \
		install -c -m 755 xmlman/xml2man $(SYMROOT)$(bindir)/xml2man; \
		dsymutil -o $(SYMROOT)$(bindir)/xml2man.dSYM $(SYMROOT)$(bindir)/xml2man; \
		install -c -m 755 xmlman/hdxml2manxml $(SYMROOT)$(bindir)/hdxml2manxml; \
		dsymutil -o $(SYMROOT)$(bindir)/hdxml2manxml.dSYM $(SYMROOT)$(bindir)/hdxml2manxml; \
		install -c -m 755 xmlman/resolveLinks $(SYMROOT)$(bindir)/resolveLinks; \
		dsymutil -o $(SYMROOT)$(bindir)/resolveLinks.dSYM $(SYMROOT)$(bindir)/resolveLinks ; \
	fi
	install -s -c -m 755 xmlman/xml2man $(DSTROOT)$(bindir)/xml2man
	install -s -c -m 755 xmlman/hdxml2manxml $(DSTROOT)$(bindir)/hdxml2manxml
	install -s -c -m 755 xmlman/resolveLinks $(DSTROOT)$(bindir)/resolveLinks
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

	# Install test suite
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite/parser_tests
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite/resolvelinks
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite/resolvelinks/sourcefiles
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite/resolvelinks/tests
	umask 022 && install -d $(DSTROOT)/usr/share/headerdoc/testsuite/c_preprocessor_tests
	install -c -m 444 testsuite/parser_tests/*.test $(DSTROOT)/usr/share/headerdoc/testsuite/parser_tests

	# Install resolvelinks test tools
	install -c -m 755 testsuite/resolvelinks/update.sh $(DSTROOT)/usr/share/headerdoc/testsuite/resolvelinks
	install -c -m 755 testsuite/resolvelinks/runtests.sh $(DSTROOT)/usr/share/headerdoc/testsuite/resolvelinks

	# Make resolvelinks test source directories
	find testsuite/resolvelinks/sourcefiles -type d -and \! -path '*/CVS/*' -and \! -path '*/CVS' -exec install -m 755 -d -c {} $(DSTROOT)/usr/share/headerdoc/{} \;
	# Copy resolvelinks test sources
	find testsuite/resolvelinks/sourcefiles \! -type d -and \! -path '*/CVS/*' -and \! -path '*/CVS' -exec install -m 444 -c {} $(DSTROOT)/usr/share/headerdoc/{} \;

	# Make resolvelinks test expected result directories
	find testsuite/resolvelinks/tests -type d \! -path '*/CVS/*' -and \! -path '*/CVS' -exec install -m 755 -d -c {} $(DSTROOT)/usr/share/headerdoc/{} \;

	# Copy resolvelinks test expected results
	find testsuite/resolvelinks/tests \! -type d -and \! -path '*/CVS/*' -and \! -path '*/CVS' -exec install -m 444 -c {} $(DSTROOT)/usr/share/headerdoc/{} \;


	install -c -m 444 testsuite/c_preprocessor_tests/*.test $(DSTROOT)/usr/share/headerdoc/testsuite/c_preprocessor_tests

	# Install copies everywhere else
	if [ -f "/usr/local/versioner/perl/versions" ] ; then \
		./installmulti.sh "$(DSTROOT)"; \
	fi

apidoc:
	./generateAPIDocs.sh
	./apicoverage.sh

