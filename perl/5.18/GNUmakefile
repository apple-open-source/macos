##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.18.2
COMPATVERSIONS =
MODSUFFIX = -mod
#TARVERSION :=

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
export ENV_PERL_ARCHNAME := darwin-thread-multi-2level
export ENV_PERL_BASE_VERSION := $(basename $(VERSION))
export ENV_PERL_VERSION := $(VERSION)

FIX = fix
ifdef TARVERSION
PROJVERS = $(PROJECT)-$(TARVERSION)
else
PROJVERS = $(PROJECT)-$(VERSION)
endif
ifeq ($(MAKECMDGOALS),custominstallsrc)
TARBALLPATTERN := $(PROJVERS).tar.*
RETARBALL = $(subst .tar.,$(MODSUFFIX).tar.,$(TARBALL))
else
TARBALLPATTERN := $(PROJVERS)$(MODSUFFIX).tar.*
endif
TARBALL := $(shell cd '$(SRCROOT)' && ls $(TARBALLPATTERN))
ifeq "$(suffix $(TARBALL))" ".bz2"
TARARGS := xojf
ifeq ($(MAKECMDGOALS),custominstallsrc)
RETARARGS := cjf
endif
else
TARARGS := xozf
ifeq ($(MAKECMDGOALS),custominstallsrc)
RETARARGS := czf
endif
endif

ARCHLIB = $(ENV_PERL)/$(ENV_PERL_BASE_VERSION)/$(ENV_PERL_ARCHNAME)
EXTRAS = $(ENV_PERL)/Extras
EXTRASARCH = $(EXTRASLIB)/$(ENV_PERL_ARCHNAME)
EXTRASLIB = $(EXTRAS)/$(ENV_PERL_BASE_VERSION)
LIBRARYPERL = /Library/Perl/$(ENV_PERL_BASE_VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
NONLP = No_Network_Library_Perl
PRIVLIB = $(ENV_PERL)/$(ENV_PERL_BASE_VERSION)

UPDATES := /Library/Perl/Updates
UPDATESARCH := $(UPDATES)/$(ENV_PERL_VERSION)/$(ENV_PERL_ARCHNAME)
UPDATESLIB := $(UPDATES)/$(ENV_PERL_VERSION)

no_target: $(OBJROOT)/$(PROJECT) build

build:
	$(MAKE) -C '$(OBJROOT)' -f Makefile SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
		NONLP='$(NONLP)' \
		EXTRASLIB='$(EXTRASLIB)' EXTRASARCH='$(EXTRASARCH)' \
		UPDATESLIB='$(UPDATESLIB)' UPDATESARCH='$(UPDATESARCH)' \
		DESTDIR=$(DSTROOT)

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install: $(OBJROOT)/$(PROJECT) installperl installperlupdates installstrip fixupperl installopensource

ifeq ($(MAKECMDGOALS),install)
PLIST = $(OSV)/$(PROJECT).plist
PROJECTS := $(shell $(MAKE) -f '$(OBJROOT)/updates/Makefile' projects)
installopensource:
	install -d $(DSTROOT)$(OSV)
ifeq ($(words $(PROJECTS)),0)
	install -m 0644 $(SRCROOT)/$(PROJECT).plist $(DSTROOT)$(OSV)
else
	echo '<plist version="1.0">' > $(DSTROOT)$(PLIST)
	echo '<array>' >> $(DSTROOT)$(PLIST)
	sed -e '/^<\/*plist/d' -e 's/^/	/' $(SRCROOT)/$(PROJECT).plist >> $(DSTROOT)$(PLIST)
	@set -x && \
	for p in $(PROJECTS); do \
	    sed -e 's/^/	/' $(SRCROOT)/updates/Modules/$$p/oss.partial >> $(DSTROOT)$(PLIST) || exit 1; \
	done
	echo '</array>' >> $(DSTROOT)$(PLIST)
	echo '</plist>' >> $(DSTROOT)$(PLIST)
	chmod 0644 $(DSTROOT)$(PLIST)
endif
	install -d $(DSTROOT)$(OSL)
	install -m 0644 $(OBJROOT)/$(PROJECT)/Artistic $(DSTROOT)$(OSL)/perl.txt
endif

ifneq "$(RC_XBS)" "YES"
MOREARGS += GnuNoBuild=YES
endif
ifneq "$(shell id -u)" "0"
MOREARGS += GnuNoChown=YES
endif
installperl::
	$(MAKE) -C '$(OBJROOT)' -f Makefile install SRCROOT='$(OBJROOT)' \
		OBJROOT='$(OBJROOT)/$(PROJECT)' DSTROOT='$(DSTROOT)' \
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
		NONLP='$(NONLP)' \
		EXTRASLIB='$(EXTRASLIB)' EXTRASARCH='$(EXTRASARCH)' \
		UPDATESLIB='$(UPDATESLIB)' UPDATESARCH='$(UPDATESARCH)' \
		DESTDIR=$(DSTROOT) $(MOREARGS)
	sed -e 's/@ARCHFLAGS@/$(ARCHFLAGS)/g' \
	    -e 's,@ARCHLIB@,$(ARCHLIB),g' \
	    -e 's,@EXTRASARCH@,$(EXTRASARCH),g' \
	    -e 's,@EXTRASLIB@,$(EXTRASLIB),g' \
	    -e 's,@PRIVLIB@,$(PRIVLIB),g' \
	    -e 's,@UPDATESARCH@,$(UPDATESARCH),g' \
	    -e 's,@UPDATESLIB@,$(UPDATESLIB),g' \
	    '$(SRCROOT)/fix/Config_heavy.pl.ex' \
	| ex - '$(OBJROOT)/$(PROJECT)/lib/Config_heavy.pl'
	sed -e 's/@ARCHFLAGS@/$(ARCHFLAGS)/g' \
	    -e 's,@ARCHLIB@,$(ARCHLIB),g' \
	    -e 's,@EXTRASARCH@,$(EXTRASARCH),g' \
	    -e 's,@EXTRASLIB@,$(EXTRASLIB),g' \
	    -e 's,@PRIVLIB@,$(PRIVLIB),g' \
	    -e 's,@UPDATESARCH@,$(UPDATESARCH),g' \
	    -e 's,@UPDATESLIB@,$(UPDATESLIB),g' \
	    '$(SRCROOT)/fix/Config_heavy.pl.ex' \
	| ex - '$(DSTROOT)/$(ENV_PERL)/$(ENV_PERL_BASE_VERSION)/$(ENV_PERL_ARCHNAME)/Config_heavy.pl'
	install -d '$(DSTROOT)$(ENV_UPDATESLIB)'
	cp $(UPDATES_README) $(DSTROOT)$(ENV_UPDATESLIB)/ReadMe.txt

installperlupdates:
	DYLD_LIBRARY_PATH='$(DSTROOT)$(ARCHLIB)/CORE' \
	    PATH='$(DSTROOT)/usr/bin:$(PATH)' PERL5LIB='$(DSTROOT)$(PRIVLIB)' \
	    $(MAKE) -C '$(OBJROOT)/updates' install \
	    OBJROOT='$(OBJROOT)/updates' DSTROOT='$(DSTROOT)' \
	    SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'

DSYMUTIL := $(shell xcrun -find dsymutil)
installstrip:
	@set -x && \
	cd $(DSTROOT) && \
	find . -type f | xargs file | fgrep 'Mach-O' | grep -v '):[ 	]' | sed -e 's/: .*//' -e 's,^\./,,' > $(SYMROOT)/strip.list && \
	cpio -pdmv $(SYMROOT) < $(SYMROOT)/strip.list && \
	strip -x `cat $(SYMROOT)/strip.list`
	@set -x && \
	cd $(SYMROOT) && \
	for i in `cat strip.list`; do \
	    $(DSYMUTIL) $$i || exit 1; \
	done

fixupperl:
	install -d '$(DSTROOT)$(LIBRARYPERL)'
	echo '$(EXTRASLIB)' > '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'
	@set -x && for i in $(COMPATVERSIONS); do \
		echo /Library/Perl/$$i >> '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'; \
	done
	touch '$(DSTROOT)$(LIBRARYPERL)/$(NONLP)'

##---------------------------------------------------------------------
# 6476113: doio.c.ed and perlio.c.ed are used to fix this bug, which should
# hopefully be fixed in a future version of perl.
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT)::
	@set -x && \
	cd '$(OBJROOT)' && \
	gnutar $(TARARGS) '$(TARBALL)' && \
	rm -rf $(PROJECT) && \
	mv $(PROJVERS) $(PROJECT) && \
	cat hints.append >> $(PROJECT)/hints/darwin.sh && \
	ed - $(PROJECT)/Configure < fix/Configure.ed && \
	ed - $(PROJECT)/cpan/Compress-Raw-Bzip2/Makefile.PL < fix/cpan_Compress-Raw-Bzip2_Makefile.PL.ed && \
	ed - $(PROJECT)/cpan/Compress-Raw-Zlib/Makefile.PL < fix/cpan_Compress-Raw-Zlib_Makefile.PL.ed && \
	ed - $(PROJECT)/cpan/DB_File/config.in < fix/cpan_DB_File_config.in.ed && \
	ed - $(PROJECT)/cpan/DB_File/Makefile.PL < fix/cpan_DB_File_Makefile.PL.ed && \
	ed - $(PROJECT)/cpan/Pod-Perldoc/lib/Pod/Perldoc.pm < fix/lib_Pod_Perldoc.pm.ed && \
	ed - $(PROJECT)/cpan/Pod-Perldoc/lib/Pod/Perldoc/ToMan.pm < fix/lib_Pod_Perldoc_ToMan.pm.ed && \
	ed - $(PROJECT)/doio.c < fix/doio.c.ed && \
	ed - $(PROJECT)/ext/DynaLoader/dl_dlopen.xs < fix/dl_dlopen.xs.ed && \
	ed - $(PROJECT)/handy.h < fix/handy.h.ed && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin.sh.ed && \
	ed - $(PROJECT)/patchlevel.h < fix/patchlevel.h.ed && \
	ed - $(PROJECT)/perl.c < fix/perl.c.ed && \
	ed - $(PROJECT)/perlio.c < fix/perlio.c.ed && \
	ed - $(PROJECT)/pp_sys.c < fix/pp_sys.c.ed && \
	sed -e 's/@VERSION@/$(VERSION)/g' \
	    -e 's/@VERSION_DEFAULT@/$(ENV_PERL_BASE_VERSION)/g' \
	    -e 's/@VERSION_ALT@/$(ENV_VERSION_ALT)/g' \
	    fix/README.macosx.ed | ed - $(PROJECT)/README.macosx

ifneq "$(RC_XBS)" "YES"
clean:
	rm -rf $(OBJROOT) $(SYMROOT)
endif

##---------------------------------------------------------------------
# perly.h is generated with a GPL3+special-exception license.  So we unpack
# the tarball, run "perl regen_perly.pl" to regenerate perly.h (with
# GPL2+special-exception license from bison 2.3), and repack.
##---------------------------------------------------------------------
custominstallsrc:
	gnutar $(TARARGS) '$(TARBALL)'
	(cd $(PROJVERS) && perl regen_perly.pl)
	gnutar $(RETARARGS) '$(RETARBALL)' $(PROJVERS)
	rm -rf $(PROJVERS) '$(TARBALL)'

GNUmakefile_inc := $(TOPSRCROOT)/Platforms/$(RC_TARGET_CONFIG)/$(ENV_PERL_BASE_VERSION)/GNUmakefile.inc
ifneq ($(wildcard $(GNUmakefile_inc)),)
include $(GNUmakefile_inc)
endif

.DEFAULT:
	@$(MAKE) -f Makefile $@
