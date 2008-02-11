##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.8.8

RSYNC = rsync -rlpt
PWD = $(shell pwd)
ifndef DSTROOT
ifdef DESTDIR
export DSTROOT = $(shell mkdir -p '$(DESTDIR)' && echo '$(DESTDIR)')
else
export DSTROOT = /
endif
endif
ifndef OBJROOT
export OBJROOT = $(shell mkdir -p '$(PWD)/OBJROOT' && echo '$(PWD)/OBJROOT')
RSYNC += --exclude=OBJROOT
endif
ifndef SRCROOT
export SRCROOT = $(PWD)
endif
ifndef SYMROOT
export SYMROOT = $(shell mkdir -p '$(PWD)/SYMROOT' && echo '$(PWD)/SYMROOT')
RSYNC += --exclude=SYMROOT
endif
ifndef RC_ARCHS
export RC_ARCHS = $(shell arch)
export RC_$(RC_ARCHS) = YES
endif
ifndef RC_CFLAGS
export RC_CFLAGS = $(foreach A,$(RC_ARCHS),-arch $(A)) $(RC_NONARCH_CFLAGS)
endif
ifndef RC_NONARCH_CFLAGS
export RC_NONARCH_CFLAGS = -pipe
endif
ifndef RC_ProjectName
export RC_ProjectName = $(PROJECT)
endif

BASE = $(basename $(VERSION))
ARCHLIB = darwin-thread-multi-2level
FIX = $(SRCROOT)/fix
PROJVERS = $(PROJECT)-$(VERSION)
TARBALL = $(PROJVERS).tar.bz2
EXTRAS = $(DSTROOT)/System/Library/Perl/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
LIBRARYPERL = $(DSTROOT)/Library/Perl/$(VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
PERL = $(DSTROOT)/System/Library/Perl
CORE = $(ARCHLIB)/CORE
LIBPERL = libperl.dylib
LIBBASE = lib/$(BASE)
LIBPERLLINK = ../../$(VERSION)/$(CORE)/$(LIBPERL)
SLP = $(PERL)/$(VERSION)
PLDTRACE_H = $(OBJROOT)/$(PROJECT)/pldtrace.h
COMPATVERSIONS = 5.8.6 5.8.1

no_target: $(OBJROOT)/$(PROJECT) build

build:
	$(MAKE) -C '$(OBJROOT)' -f Makefile SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' DESTDIR= \
		_VERSION=$(VERSION) \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)'

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install: $(OBJROOT)/$(PROJECT) installperl installperlupdates fixup64
	install -d $(OSV)
	install $(SRCROOT)/$(PROJECT).plist $(OSV)
	install -d $(OSL)
	install $(OBJROOT)/$(PROJECT)/Artistic $(OSL)/perl.txt

ifneq "$(RC_XBS)" "YES"
MOREARGS += GnuNoBuild=YES
endif
ifneq "$(shell id -u)" "0"
MOREARGS += GnuNoChown=YES
endif
installperl:
	$(MAKE) -C '$(OBJROOT)' -f Makefile install SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' _VERSION=$(VERSION) \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
		DESTDIR= $(MOREARGS)
	install -d '$(LIBRARYPERL)'
	echo '$(subst $(DSTROOT),,$(EXTRASPERL))' > '$(LIBRARYPERL)/$(APPENDFILE)'
	@set -x && for i in $(COMPATVERSIONS); do \
		echo /Library/Perl/$$i >> '$(LIBRARYPERL)/$(APPENDFILE)'; \
	done
	install -d '$(PERL)/$(LIBBASE)'
	ln -s '$(LIBPERLLINK)' '$(PERL)/$(LIBBASE)/$(LIBPERL)'
	@set -x && \
	eval `/usr/bin/stat -s '$(SLP)/$(CORE)/$(LIBPERL)'` && \
	chmod u+w '$(SLP)/$(CORE)/$(LIBPERL)' && \
	install_name_tool -id '/System/Library/Perl/$(LIBBASE)/$(LIBPERL)' '$(SLP)/$(CORE)/$(LIBPERL)' && \
	chmod $$st_mode '$(SLP)/$(CORE)/$(LIBPERL)'

installperlupdates:
	$(MAKE) -C '$(OBJROOT)/updates' install \
	    OBJROOT='$(OBJROOT)/updates' DSTROOT='$(DSTROOT)' \
	    SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'

##---------------------------------------------------------------------
# We patch hints/darwin.sh to install in $(DSTROOT), and to force putting
# things in the right place.
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	$(RSYNC) '$(SRCROOT)/' '$(OBJROOT)'
	@set -x && \
	cd '$(OBJROOT)' && \
	bzcat '$(TARBALL)' | gnutar xf - && \
	rm -rf $(PROJECT) && \
	mv $(PROJVERS) $(PROJECT) && \
	chmod u+w $(PROJECT)/hints/darwin.sh $(PROJECT)/handy.h \
	    $(PROJECT)/lib/Pod/Perldoc.pm $(PROJECT)/perl.c && \
	cat hints.append >> $(PROJECT)/hints/darwin.sh && \
	ed - $(PROJECT)/cop.h < fix/cop.h.ed && \
	ed - $(PROJECT)/handy.h < fix/handy.h.ed && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin.sh.ed && \
	ed - $(PROJECT)/lib/Pod/Perldoc.pm < fix/lib_Pod_Perldoc.pm.ed && \
	ed - $(PROJECT)/patchlevel.h < fix/patchlevel.h.ed && \
	ed - $(PROJECT)/perl.c < fix/perl.c.ed && \
	ed - $(PROJECT)/util.c < fix/util.c.ed
	dtrace -h -s $(FIX)/pldtrace.d -o '$(PLDTRACE_H)'
	cd '$(OBJROOT)/$(PROJECT)' && patch -p0 -i $(FIX)/CVE-2007-5116-regcomp.diff

PROG64 = a2p perl perl5.8.8
fixup64:
	@set -x && a='' && \
	for i in ppc64 x86_64; do \
	    lipo $(DSTROOT)/usr/bin/$(word 1,$(PROG64)) -verify_arch $$i && a="$$a -remove $$i"; \
	done; \
	test -z "$$a" || \
	for p in $(PROG64); do \
	    ditto $(DSTROOT)/usr/bin/$$p $(SYMROOT) && \
	    lipo $(DSTROOT)/usr/bin/$$p $$a -output $(DSTROOT)/usr/bin/$$p || exit 1; \
	done

ifneq "$(RC_XBS)" "YES"
clean:
	rm -rf $(OBJROOT) $(SYMROOT)
endif

.DEFAULT:
	@$(MAKE) -f Makefile $@
