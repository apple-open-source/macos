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
FIX = fix
PROJVERS = $(PROJECT)-$(VERSION)
TARBALL = $(PROJVERS).tar.bz2
EXTRAS = /System/Library/Perl/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
LIBRARYPERL = /Library/Perl/$(VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
PERL = /System/Library/Perl
CORE = $(ARCHLIB)/CORE
LIBPERL = libperl.dylib
LIBBASE = lib/$(BASE)
LIBPERLLINK = ../../$(VERSION)/$(CORE)/$(LIBPERL)
SLP = $(PERL)/$(VERSION)
PLDTRACE_H = $(PROJECT)/pldtrace.h
COMPATVERSIONS = 5.8.6 5.8.1

UPDATES := /Library/Perl/Updates
ARCHFLAGS := $(shell perl -e 'printf "-arch %s\n", join(" -arch ", grep(!/64$$/, split(" ", $$ENV{RC_ARCHS})));')
EXTRASARCH := $(EXTRASPERL)/$(ARCHLIB)
UPDATESARCH := $(UPDATES)/$(VERSION)/$(ARCHLIB)
export ENV_UPDATESLIB := $(UPDATES)/$(VERSION)
UPDATES_README := 'fix/Updates-ReadMe.txt'

no_target: $(OBJROOT)/$(PROJECT) build

build:
	$(MAKE) -C '$(OBJROOT)' -f Makefile SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' _VERSION=$(VERSION) \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
		DESTDIR=$(DSTROOT)

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
		DESTDIR=$(DSTROOT) $(MOREARGS)
	sed -e 's/@ARCHFLAGS@/$(ARCHFLAGS)/' \
	    -e 's,@EXTRASARCH@,$(EXTRASARCH),' \
	    -e 's,@EXTRASPERL@,$(EXTRASPERL),' \
	    -e 's,@UPDATESARCH@,$(UPDATESARCH),' \
	    -e 's,@ENV_UPDATESLIB@,$(ENV_UPDATESLIB),' \
	    $(SRCROOT)/fix/Config_heavy.pl.ex \
	| ex - $(DSTROOT)$(SLP)/$(ARCHLIB)/Config_heavy.pl
	install -d '$(DSTROOT)$(ENV_UPDATESLIB)'
	cp $(SRCROOT)/$(UPDATES_README) $(DSTROOT)$(ENV_UPDATESLIB)/ReadMe.txt
	install -d '$(DSTROOT)$(LIBRARYPERL)'
	echo '$(EXTRASPERL)' > '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'
	@set -x && for i in $(COMPATVERSIONS); do \
		echo /Library/Perl/$$i >> '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'; \
	done
	install -d '$(DSTROOT)$(PERL)/$(LIBBASE)'
	ln -s '$(LIBPERLLINK)' '$(DSTROOT)$(PERL)/$(LIBBASE)/$(LIBPERL)'
	@set -x && \
	eval `/usr/bin/stat -s '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)'` && \
	chmod u+w '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)' && \
	install_name_tool -id '/System/Library/Perl/$(LIBBASE)/$(LIBPERL)' '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)' && \
	chmod $$st_mode '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)'

installperlupdates:
	$(MAKE) -C '$(OBJROOT)/updates' install \
	    OBJROOT='$(OBJROOT)/updates' DSTROOT='$(DSTROOT)' \
	    SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'

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
	sed 's/@VERSION@/$(VERSION)' fix/README.macosx.ed | ed - $(PROJECT)/README.macosx && \
	ed - $(PROJECT)/util.c < fix/util.c.ed
	dtrace -h -s $(SRCROOT)/$(FIX)/pldtrace.d -o '$(OBJROOT)/$(PLDTRACE_H)'
	cd '$(OBJROOT)/$(PROJECT)' && patch -p0 -i $(SRCROOT)/$(FIX)/CVE-2007-5116-regcomp.diff
	cd '$(OBJROOT)/$(PROJECT)' && patch -p0 -i $(SRCROOT)/$(FIX)/CVE-2008-1927-double-free_5.8.8.patch

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
