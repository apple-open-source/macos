##---------------------------------------------------------------------
# GNUmakefile for CPAN (supporting multiple versions)
##---------------------------------------------------------------------
Project = CPAN
PERLPROJECT = perl
MY_HOST := x86_64
ifdef SDKROOT
PATH := $(SDKROOT)/usr/bin:$(PATH)
endif
_VERSIONERDIR := /usr/local/versioner
# Look for /usr/local/versioner in $(SDKROOT), defaulting to /usr/local/versioner
VERSIONERDIR := $(or $(wildcard $(SDKROOT)$(_VERSIONERDIR)),$(_VERSIONERDIR))
PERLVERSIONS = $(VERSIONERDIR)/$(PERLPROJECT)/versions
PWD = $(shell pwd)
ifndef SRCROOT
export SRCROOT = $(PWD)
endif
# SrcDir is the directory of this Makefile, usually the same as SRCROOT,
# but during installsrc, it is the source directory to be copied to SRCROOT.
# We need to set up SrcDir, before MAKEFILE_LIST gets changed by including
# Common.make.
SrcDir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifndef RC_TARGET_CONFIG
export RC_TARGET_CONFIG := MacOSX
endif
include $(SrcDir)/Platforms/$(RC_TARGET_CONFIG)/GNUmakefile.inc

MYFIX = $(SRCROOT)/fix
VERSIONS = $(sort $(KNOWNVERSIONS) $(BOOTSTRAPPERL))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(VERSIONERDIR)/$(PERLPROJECT) -I$(MYFIX) -framework CoreFoundation $(EXTRAVERSIONERFLAGS)

RSYNC = rsync -rlpt
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
ifndef SYMROOT
export SYMROOT = $(shell mkdir -p '$(PWD)/SYMROOT' && echo '$(PWD)/SYMROOT')
RSYNC += --exclude=SYMROOT
endif
ifndef RC_ARCHS
export RC_ARCHS := $(MY_ARCHS)
endif
ifndef RC_NONARCH_CFLAGS
export RC_NONARCH_CFLAGS = -pipe
endif
MY_CFLAGS := $(foreach A,$(MY_ARCHS),-arch $(A)) $(RC_NONARCH_CFLAGS)
ifndef RC_CFLAGS
export RC_CFLAGS = $(MY_CFLAGS)
endif
ifndef RC_ProjectName
export RC_ProjectName = $(Project)
endif

FIX = $(VERSIONERDIR)/$(PERLPROJECT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

MYVERSIONBINLIST = $(OBJROOT)/usr-bin.list
MYVERSIONMANLIST = $(OBJROOT)/usr-share-man.list
VERSIONER_C = $(VERSIONERDIR)/versioner.c
VERSIONBINLIST = $(VERSIONERDIR)/$(PERLPROJECT)/usr-bin.list
VERSIONMANLIST = $(VERSIONERDIR)/$(PERLPROJECT)/usr-share-man.list
PERL_ARCHNAME = $(shell perl -MConfig -e 'print $$Config::Config{archname}')
PERL_CC = $(shell perl -MConfig -e 'print $$Config::Config{cc}')

build:: $(foreach v,$(VERSIONS),$(SRCROOT)/$(v).inc)
	@set -x && \
	for vers in $(VERSIONS); do \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		VERSIONER_PERL_VERSION=$$vers \
		VERSIONER_PERL_PREFER_32_BIT=$(MY_PREFER_32_BIT) \
		CC=$(PERL_CC) \
		$(MYEXTRAENV) \
		$(MAKE) -f Makefile install \
		VERS=$$vers \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		RC_CFLAGS='$(MY_CFLAGS)' \
		RC_ARCHS='$(MY_ARCHS)' >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		touch "$(OBJROOT)/$$vers/.ok" && \
		echo "######## Finished $$vers:" `date` '########' >> "$(SYMROOT)/$$vers/LOG" 2>&1 \
	    ) & \
	done && \
	wait && \
	for vers in $(VERSIONS); do \
	    cat $(SYMROOT)/$$vers/LOG && \
	    rm -f $(SYMROOT)/$$vers/LOG || exit 1; \
	done && \
	if [ $(TESTOK) ]; then \
	    $(MAKE) merge; \
	else \
	    echo '#### error detected, not merging'; \
	    exit 1; \
	fi

MERGEBIN = /usr/bin

merge: mergebegin mergedefault mergeversions fixautodirs
ifneq ($(wildcard $(OBJROOT)/$(DEFAULT)/DSTROOT$(MERGEBIN)),)
merge: mergebin
endif
ifneq ($(EXTRAMERGE),)
merge: $(EXTRAMERGE)
endif

mergebegin:
	@echo '####### Merging #######'

TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(OBJROOT)/wrappers
	@[ ! -s $(OBJROOT)/wrappers ] || { set -x && \
	$(PERL_CC) $(MY_CFLAGS) $(VERSIONERFLAGS) $(VERSIONER_C) -o $(DSTROOT)$(TEMPWRAPPER) && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    ln -f $(DSTROOT)$(TEMPWRAPPER) $(DSTROOT)$(MERGEBIN)/$$w || exit 1; \
	done && \
	rm -f $(DSTROOT)$(TEMPWRAPPER); }

DUMMY = dummy.pl
$(OBJROOT)/wrappers:
	mkdir -p $(DSTROOT)$(MERGEBIN)
	install $(FIX)/$(DUMMY) $(DSTROOT)$(MERGEBIN)
	@set -x && \
	touch $(OBJROOT)/wrappers && \
	for vers in $(ORDEREDVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEBIN) && \
	    for f in *; do \
		if file $$f | head -1 | fgrep -q script; then \
		    fv=`echo $$f | sed -E 's/(\.[^.]*)?$$/'"$$vers&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEBIN)/$$fv && \
		    sed "s/@VERSION@/$$vers/g" $(MYFIX)/scriptvers.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv && \
		    if [ ! -e $(DSTROOT)$(MERGEBIN)/$$f ]; then \
			ln -f $(DSTROOT)$(MERGEBIN)/$(DUMMY) $(DSTROOT)$(MERGEBIN)/$$f; \
		    fi; \
		elif echo $$f | grep -q "[^0-9]$$vers"; then \
		    true; \
		else \
		    echo $$f >> $(OBJROOT)/wrappers && \
		    if [ -e $$f$$vers ]; then \
			ditto $$f$$vers $(DSTROOT)$(MERGEBIN)/$$f$$vers; \
		    else \
			ditto $$f $(DSTROOT)$(MERGEBIN)/$$f$$vers; \
		    fi \
		fi || exit 1; \
	    done || exit 1; \
	done
	rm -f $(DSTROOT)$(MERGEBIN)/$(DUMMY)
	cd $(DSTROOT)$(MERGEBIN) && \
	ls | sort > $(MYVERSIONBINLIST) && \
	rm -fv `comm -12 $(VERSIONBINLIST) $(MYVERSIONBINLIST)`

MERGEDEFAULT = \
    Developer
mergedefault:
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && \
	{ [ ! -d $(MERGEDEFAULT) ] || rsync -Ra $(MERGEDEFAULT) $(DSTROOT); }

MERGEMAN = /usr/share/man
mergeman:
	@set -x && \
	for vers in $(ORDEREDVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEMAN) && \
	    for d in man*; do \
		cd $$d && \
		for f in *.*; do \
		    ff=`echo $$f | sed -E "s/\.[^.]*(\.gz)?$$/$$vers&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEMAN)/$$d/$$ff && \
		    if [ ! -e $(DSTROOT)$(MERGEMAN)/$$d/$$f ]; then \
			ditto $$f $(DSTROOT)$(MERGEMAN)/$$d/$$f; \
		    fi || exit 1; \
		done && \
		cd .. || exit 1; \
	    done || exit 1; \
	done
	cd $(DSTROOT)$(MERGEMAN) && \
	find . ! -type d | sed 's,^\./,,' | sort > $(MYVERSIONMANLIST) && \
	rm -fv `comm -12 $(VERSIONMANLIST) $(MYVERSIONMANLIST)`

MERGEVERSIONS = \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done

# 11098667: Unconditionally remove the $(PERL_ARCHNAME)/auto/share/dist
# directories (empty share directory may be removed by the following change).
# 12785350: The two modules Syntax-Keyword-Junction and syntax, install a
# Syntax directory and a syntax.pm file, respectively.  The auto mechanism
# then creates a Syntax and syntax directory in the auto directory.  This
# triggers the case-insensitive verifier.  Since these directories are
# empty (or contain empty directories), we can just remove all these empty
# directories.
AUTO = auto $(PERL_ARCHNAME)/auto
fixautodirs:
	rm -rf $(foreach v,$(VERSIONS),$(DSTROOT)/System/Library/Perl/Extras/$(v)/$(PERL_ARCHNAME)/auto/share/dist)
	perl $(MYFIX)/removeemptyautodirs $(foreach v,$(VERSIONS),$(foreach a,$(AUTO),$(DSTROOT)/System/Library/Perl/Extras/$(v)/$(a)))

OSL = $(DSTROOT)/usr/local/OpenSourceLicenses
OSV = $(DSTROOT)/usr/local/OpenSourceVersions
PLIST = $(OSV)/$(Project).plist
LICENSE = $(OSL)/$(Project).txt
MODULELIST = $(OBJROOT)/ModuleList
mergeoss:
	@set -x && \
	for i in $(VERSIONS); do \
	    sed -n '/^    /{s/^ *//;s/ \\$$//;p;}' $(SRCROOT)/$$i.inc || exit 1; \
	done | sort -u > $(MODULELIST)
	mkdir -p $(OSL) $(OSV)
	@echo 'Creating $(PLIST):'
	@echo '<?xml version="1.0" encoding="UTF-8"?>' > $(PLIST)
	@echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $(PLIST)
	@echo '<plist version="1.0">' >> $(PLIST)
	@echo '<array>' >> $(PLIST)
	@cd $(SRCROOT)/Modules && \
	for m in `cat $(MODULELIST)`; do \
	    echo adding $$m && \
	    sed 's/^/	/' $$m/oss.partial >> $(PLIST) && \
	    echo "======================== $$m ========================" >> $(LICENSE) && \
	    cat $$m/LICENSE >> $(LICENSE) || exit 1; \
	done
	@echo '</array>' >> $(PLIST)
	@echo '</plist>' >> $(PLIST)
