##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.8.9
COMPATVERSIONS = 5.8.8 5.8.6 5.8.1
#TARVERSION =

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
endif
ifndef SRCROOT
export SRCROOT = $(PWD)
endif
ifndef SYMROOT
export SYMROOT = $(shell mkdir -p '$(PWD)/SYMROOT' && echo '$(PWD)/SYMROOT')
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
export ENV_PERL := /System/Library/Perl
export ENV_PERL_VERSION := $(VERSION)
export ENV_PERL_ARCHNAME := darwin-thread-multi-2level

BASE = $(basename $(VERSION))
FIX = fix
ifdef TARVERSION
PROJVERS = $(PROJECT)-$(TARVERSION)
else
PROJVERS = $(PROJECT)-$(VERSION)
endif
TARBALL := $(shell cd '$(SRCROOT)' && ls $(PROJVERS).tar.*)
ifeq "$(suffix $(TARBALL))" ".bz2"
TARARGS := xojf
else
TARARGS := xozf
endif
EXTRAS = $(ENV_PERL)/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
LIBRARYPERL = /Library/Perl/$(VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
CORE = $(ENV_PERL_ARCHNAME)/CORE
LIBPERL = libperl.dylib
LIBBASE = lib/$(BASE)
LIBPERLLINK = ../../$(VERSION)/$(CORE)/$(LIBPERL)
SLP = $(ENV_PERL)/$(VERSION)
PLDTRACE_H = $(PROJECT)/pldtrace.h
NOOP = @true

UPDATES := /Library/Perl/Updates
ARCHFLAGS := $(shell perl -e 'printf "-arch %s\n", join(" -arch ", grep(!/64$$/, split(" ", $$ENV{RC_ARCHS})));')
EXTRASARCH := $(EXTRASPERL)/$(ENV_PERL_ARCHNAME)
UPDATESARCH := $(UPDATES)/$(VERSION)/$(ENV_PERL_ARCHNAME)
export ENV_UPDATESLIB := $(UPDATES)/$(VERSION)

no_target: $(OBJROOT)/$(PROJECT) build

build:
	$(MAKE) -C '$(OBJROOT)' -f Makefile SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' _VERSION=$(VERSION) \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
		DESTDIR=$(DSTROOT)

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install: $(OBJROOT)/$(PROJECT) installperl installperlupdates installstrip fixupperl installopensource compressmanpages fixup64

installopensource:
	install -d $(DSTROOT)$(OSV)
	install $(SRCROOT)/$(PROJECT).plist $(DSTROOT)$(OSV)
	install -d $(DSTROOT)$(OSL)
	install $(OBJROOT)/$(PROJECT)/Artistic $(DSTROOT)$(OSL)/perl.txt

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
	| ex - $(DSTROOT)$(SLP)/$(ENV_PERL_ARCHNAME)/Config_heavy.pl
	install -d '$(DSTROOT)$(ENV_UPDATESLIB)'
	cp $(UPDATES_README) $(DSTROOT)$(ENV_UPDATESLIB)/ReadMe.txt

installstrip:
	@set -x && \
	cd $(DSTROOT) && \
	find . -type f | xargs file | fgrep 'Mach-O' | grep -v '):[ 	]' | sed -e 's/: .*//' -e 's,^\./,,' > $(SYMROOT)/strip.list && \
	cpio -pdmv $(SYMROOT) < $(SYMROOT)/strip.list && \
	strip -x `cat $(SYMROOT)/strip.list`

fixupperl:
	install -d '$(DSTROOT)$(LIBRARYPERL)'
	echo '$(subst $(DSTROOT),,$(DSTROOT)$(EXTRASPERL))' > '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'
	@set -x && for i in $(COMPATVERSIONS); do \
		echo /Library/Perl/$$i >> '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'; \
	done
	install -d '$(DSTROOT)$(ENV_PERL)/$(LIBBASE)'
	ln -s '$(LIBPERLLINK)' '$(DSTROOT)$(ENV_PERL)/$(LIBBASE)/$(LIBPERL)'
	@set -x && \
	eval `/usr/bin/stat -s '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)'` && \
	chmod u+w '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)' && \
	install_name_tool -id '/System/Library/Perl/$(LIBBASE)/$(LIBPERL)' '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)' && \
	chmod $$st_mode '$(DSTROOT)$(SLP)/$(CORE)/$(LIBPERL)'

installperlupdates:
	$(MAKE) -C '$(OBJROOT)/updates' install \
	    OBJROOT='$(OBJROOT)/updates' DSTROOT='$(DSTROOT)' \
	    SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'

compressmanpages:
	/Developer/Makefiles/bin/compress-man-pages.pl "$(DSTROOT)/usr/share/man"

$(OBJROOT)/$(PROJECT):
	@set -x && \
	cd '$(OBJROOT)' && \
	gnutar $(TARARGS) '$(TARBALL)' && \
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
	ed - $(PROJECT)/perlio.c < fix/perlio.c.ed && \
	sed -e 's/@VERSION@/$(VERSION)/g' \
	    -e 's/@VERSION5_8@/$(ENV_VERSION5_8)/g' \
	    -e 's/@VERSION5_10@/$(ENV_VERSION5_10)/g' \
	    fix/README.macosx.ed | ed - $(PROJECT)/README.macosx
	dtrace -h -s $(SRCROOT)/$(FIX)/pldtrace.d -o '$(OBJROOT)/$(PLDTRACE_H)'
	@set -x && \
	cd '$(OBJROOT)' && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin42.sh.ed

PROG64 = a2p perl perl$(VERSION)
fixup64:
	@set -x && a='' && \
	for i in ppc64 x86_64; do \
	    lipo $(DSTROOT)/usr/bin/$(word 1,$(PROG64)) -verify_arch $$i && a="$$a -remove $$i"; \
	done; \
	test -z "$$a" || \
	for p in $(PROG64); do \
	    lipo $(DSTROOT)/usr/bin/$$p $$a -output $(DSTROOT)/usr/bin/$$p || exit 1; \
	done

ifneq "$(RC_XBS)" "YES"
clean:
	rm -rf $(OBJROOT) $(SYMROOT)
endif

.DEFAULT:
	@$(MAKE) -f Makefile $@
