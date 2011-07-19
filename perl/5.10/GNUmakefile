##---------------------------------------------------------------------
# GNUmakefile for perl
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = perl
VERSION = 5.10.1
##---------------------------------------------------------------------
# 8097042: To transition from 3-number to 2-number version names, we need
# to keep a backward-compatible reference to the 3-number /Library/Perl
# directory 5.10.1
##---------------------------------------------------------------------
COMPATVERSIONS = 5.10.1 5.10.0
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
export ENV_PERL_ARCHNAME := darwin-thread-multi-2level
export ENV_PERL_BASE_VERSION := $(basename $(VERSION))
export ENV_PERL_VERSION := $(VERSION)

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

ARCHLIB = $(ENV_PERL)/$(ENV_PERL_BASE_VERSION)/$(ENV_PERL_ARCHNAME)
EXTRAS = $(ENV_PERL)/Extras
EXTRASARCH = $(EXTRASLIB)/$(ENV_PERL_ARCHNAME)
EXTRASLIB = $(EXTRAS)/$(ENV_PERL_BASE_VERSION)
LIBRARYPERL = /Library/Perl/$(ENV_PERL_BASE_VERSION)
APPENDFILE = AppendToPath
PREPENDFILE = PrependToPath
CORE = $(ENV_PERL_ARCHNAME)/CORE
LIBPERL = libperl.dylib
LIBBASE = lib/$(ENV_PERL_BASE_VERSION)
LIBPERLLINK = ../../$(ENV_PERL_BASE_VERSION)/$(CORE)/$(LIBPERL)
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
		EXTRASLIB='$(EXTRASLIB)' EXTRASARCH='$(EXTRASARCH)' \
		UPDATESLIB='$(UPDATESLIB)' UPDATESARCH='$(UPDATESARCH)' \
		DESTDIR=$(DSTROOT)

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install: $(OBJROOT)/$(PROJECT) installperl installperlupdates installstrip fixupperl installopensource compressmanpages

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
		SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
		PREPENDFILE='$(PREPENDFILE)' APPENDFILE='$(APPENDFILE)' \
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

installstrip:
	@set -x && \
	cd $(DSTROOT) && \
	find . -type f | xargs file | fgrep 'Mach-O' | grep -v '):[ 	]' | sed -e 's/: .*//' -e 's,^\./,,' > $(SYMROOT)/strip.list && \
	cpio -pdmv $(SYMROOT) < $(SYMROOT)/strip.list && \
	strip -x `cat $(SYMROOT)/strip.list`

##---------------------------------------------------------------------
# 8097042: To transition from 3-number to 2-number version names, we need
# to keep a backward-compatible reference to the 3-number Extras directory
# (the second "echo" statement below).
##---------------------------------------------------------------------
fixupperl:
	install -d '$(DSTROOT)$(LIBRARYPERL)'
	echo '$(EXTRASLIB)' > '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'
	echo '$(EXTRAS)/$(VERSION)' >> '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'
	@set -x && for i in $(COMPATVERSIONS); do \
		echo /Library/Perl/$$i >> '$(DSTROOT)$(LIBRARYPERL)/$(APPENDFILE)'; \
	done
	install -d '$(DSTROOT)$(ENV_PERL)/$(LIBBASE)'
	ln -s '$(LIBPERLLINK)' '$(DSTROOT)$(ENV_PERL)/$(LIBBASE)/$(LIBPERL)'
	install_name_tool -id '$(ENV_PERL)/$(LIBBASE)/$(LIBPERL)' '$(DSTROOT)$(PRIVLIB)/$(CORE)/$(LIBPERL)'

compressmanpages:
	/Developer/Makefiles/bin/compress-man-pages.pl "$(DSTROOT)/usr/share/man"

##---------------------------------------------------------------------
# 6476113: doio.c.ed and perlio.c.ed are used to fix this bug, which should
# hopefully be fixed in a future version of perl.
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	@set -x && \
	cd '$(OBJROOT)' && \
	gnutar $(TARARGS) '$(TARBALL)' && \
	rm -rf $(PROJECT) && \
	mv $(PROJVERS) $(PROJECT) && \
	chmod u+w $(PROJECT)/hints/darwin.sh $(PROJECT)/handy.h \
	    $(PROJECT)/lib/Pod/Perldoc.pm $(PROJECT)/perl.c && \
	cat hints.append >> $(PROJECT)/hints/darwin.sh && \
	ed - $(PROJECT)/doio.c < fix/doio.c.ed && \
	ed - $(PROJECT)/ext/DynaLoader/dl_dlopen.xs < fix/dl_dlopen.xs.ed && \
	ed - $(PROJECT)/handy.h < fix/handy.h.ed && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin.sh.ed && \
	ed - $(PROJECT)/lib/CPAN/HandleConfig.pm < fix/lib_CPAN_HandleConfig.pm.ed && \
	ed - $(PROJECT)/lib/Pod/Perldoc.pm < fix/lib_Pod_Perldoc.pm.ed && \
	ed - $(PROJECT)/lib/Pod/Perldoc/ToMan.pm < fix/lib_Pod_Perldoc_ToMan.pm.ed && \
	ed - $(PROJECT)/patchlevel.h < fix/patchlevel.h.ed && \
	ed - $(PROJECT)/perl.c < fix/perl.c.ed && \
	ed - $(PROJECT)/perlio.c < fix/perlio.c.ed && \
	sed -e 's/@VERSION@/$(VERSION)/g' \
	    -e 's/@VERSION_DEFAULT@/$(ENV_PERL_BASE_VERSION)/g' \
	    -e 's/@VERSION_ALT@/$(ENV_VERSION_ALT)/g' \
	    fix/README.macosx.ed | ed - $(PROJECT)/README.macosx && \
	ed - $(PROJECT)/t/op/groups.t < fix/t_op_groups.t.ed
	@set -x && \
	cd '$(OBJROOT)' && \
	ed - $(PROJECT)/hints/darwin.sh < fix/darwin42.sh.ed && \
	ed - $(PROJECT)/makedepend.SH < fix/makedepend.SH.ed

ifneq "$(RC_XBS)" "YES"
clean:
	rm -rf $(OBJROOT) $(SYMROOT)
endif

# NOOP
custominstallsrc:
	@true

.DEFAULT:
	@$(MAKE) -f Makefile $@
