##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.10.0
COMPATVERSIONS =
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

UPDATES := /Library/Perl/Updates
ARCHFLAGS := $(shell perl -e 'printf "-arch %s\n", join(" -arch ", split(" ", $$ENV{RC_ARCHS}));')
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

install: $(OBJROOT)/$(PROJECT) installperl installstrip fixupperl installopensource compressmanpages

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

compressmanpages:
	/Developer/Makefiles/bin/compress-man-pages.pl "$(DSTROOT)/usr/share/man"

##---------------------------------------------------------------------
# 6476113: doio.c.ed and perlio.c.ed are used to fix this bug, which should
# hopefully be fixed in a future version of perl.
##---------------------------------------------------------------------
COMPRESSRAWZLIB = Compress-Raw-Zlib-2.020
$(OBJROOT)/$(PROJECT):
	@set -x && \
	cd '$(OBJROOT)' && \
	gnutar $(TARARGS) '$(TARBALL)' && \
	rm -rf $(PROJECT) && \
	mv $(PROJVERS) $(PROJECT) && \
	chmod u+w $(PROJECT)/hints/darwin.sh $(PROJECT)/handy.h \
	    $(PROJECT)/lib/Pod/Perldoc.pm $(PROJECT)/perl.c && \
	cat hints.append >> $(PROJECT)/hints/darwin.sh && \
	ed - $(PROJECT)/ext/Compress/Raw/Zlib/config.in < fix/ext_Compress_Raw_Zlib_config.in.ed && \
	ed - $(PROJECT)/doio.c < fix/doio.c.ed && \
	ed - $(PROJECT)/ext/DynaLoader/dl_dlopen.xs < fix/dl_dlopen.xs.ed && \
	ed - $(PROJECT)/cop.h < fix/cop.h.ed && \
	ed - $(PROJECT)/handy.h < fix/handy.h.ed && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin.sh.ed && \
	ed - $(PROJECT)/lib/CPAN/HandleConfig.pm < fix/lib_CPAN_HandleConfig.pm.ed && \
	ed - $(PROJECT)/lib/Pod/Perldoc.pm < fix/lib_Pod_Perldoc.pm.ed && \
	ed - $(PROJECT)/patchlevel.h < fix/patchlevel.h.ed && \
	ed - $(PROJECT)/perl.c < fix/perl.c.ed && \
	ed - $(PROJECT)/perlio.c < fix/perlio.c.ed && \
	sed -e 's/@VERSION@/$(VERSION)/g' \
	    -e 's/@VERSION5_8@/$(ENV_VERSION5_8)/g' \
	    -e 's/@VERSION5_10@/$(ENV_VERSION5_10)/g' \
	    fix/README.macosx.ed | ed - $(PROJECT)/README.macosx && \
	ed - $(PROJECT)/t/op/groups.t < fix/t_op_groups.t.ed
	dtrace -h -s $(SRCROOT)/$(FIX)/pldtrace.d -o '$(OBJROOT)/$(PLDTRACE_H)'
	@set -x && \
	cd '$(OBJROOT)' && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin42.sh.ed && \
	ed - $(PROJECT)/makedepend.SH < fix/makedepend.SH.ed
	gnutar -xojf $(OBJROOT)/fix/File-Path-2.07_02.tar.bz2 -C $(OBJROOT)/$(PROJECT)
	gnutar -xozf $(OBJROOT)/fix/$(COMPRESSRAWZLIB).tar.gz -C $(OBJROOT)
	ditto $(OBJROOT)/$(COMPRESSRAWZLIB) $(OBJROOT)/$(PROJECT)/ext/Compress/Raw/Zlib

ifneq "$(RC_XBS)" "YES"
clean:
	rm -rf $(OBJROOT) $(SYMROOT)
endif

.DEFAULT:
	@$(MAKE) -f Makefile $@
