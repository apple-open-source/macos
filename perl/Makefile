##---------------------------------------------------------------------
# Makefile for perl (supporting multiple versions)
##---------------------------------------------------------------------
Project = perl

MY_HOST := x86_64
PWD := $(shell pwd)
ifndef SRCROOT
export SRCROOT = $(PWD)
endif
VERSIONER_C = $(SRCROOT)/versioner/versioner.c
VERSIONERDIR = /usr/local/versioner
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(DSTROOT)$(VERSIONERDIR)/$(Project) -I$(SRCROOT)/versioner -framework CoreFoundation $(EXTRAVERSIONERFLAGS)

# SrcDir is the directory of this Makefile, usually the same as SRCROOT,
# but during installsrc, it is the source directory to be copied to SRCROOT.
# We need to set up SrcDir, before MAKEFILE_LIST gets changed by including
# Common.make.
SrcDir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

ifndef RC_TARGET_CONFIG
export RC_TARGET_CONFIG := MacOSX
endif
include $(SrcDir)/Platforms/$(RC_TARGET_CONFIG)/Makefile.inc

VERSIONS = $(sort $(KNOWNVERSIONS) $(BOOTSTRAPPERL))
OTHERVERSIONS = $(filter-out $(DEFAULT),$(VERSIONS))
ORDEREDVERS := $(DEFAULT) $(OTHERVERSIONS)

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
export RC_CFLAGS := $(MY_CFLAGS)
endif
ifndef RC_ProjectName
export RC_ProjectName = $(Project)
endif
##---------------------------------------------------------------------
# Before, we used the versioned gcc (e.g., gcc-4.2) because newer compiler
# would occasionally be incompatible with the compiler flags that python
# records.  With clang, it doesn't use names with versions, so we just go
# back to using plain cc and c++.  With 11952207, we will automatically
# get xcrun support.
##---------------------------------------------------------------------
export MY_CC = cc

FIX = $(SRCROOT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

VERSIONVERSIONS = $(VERSIONERDIR)/$(Project)/versions
VERSIONHEADER = $(VERSIONERDIR)/$(Project)/versions.h
VERSIONBINLIST = $(VERSIONERDIR)/$(Project)/usr-bin.list
VERSIONMANLIST = $(VERSIONERDIR)/$(Project)/usr-share-man.list
VERSIONERFIX = dummy.pl scriptvers.ed

build::
	$(RSYNC) '$(SRCROOT)/' '$(OBJROOT)'
	@set -x && \
	for vers in $(VERSIONS); do \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		TOPSRCROOT='$(SRCROOT)' \
		UPDATES_README='$(SRCROOT)/fix/Updates-ReadMe.txt' \
		$(MAKE) -C "$(OBJROOT)/$$vers" install \
		SRCROOT="$(SRCROOT)/$$vers" \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		ARCHFLAGS='$(ARCHFLAGS)' \
		RC_ARCHS='$(MY_ARCHS)' \
		RC_CFLAGS='$(MY_CFLAGS)' \
		PERLVERSION=$$vers >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
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

installsrc: custominstallsrc

custominstallsrc:
	@set -x && \
	for vers in $(VERSIONS); do \
	    $(MAKE) -C "$(SRCROOT)/$$vers" custominstallsrc SRCROOT="$(SRCROOT)/$$vers" TOPSRCROOT='$(SRCROOT)' || exit 1; \
	done

merge: mergebegin mergedefault mergeversions versionerdir mergebin $(EXTRAMERGE)

mergebegin:
	@echo '####### Merging #######'

# 13896386: temporarily use "ditto" instead of "ln -f"
MERGEBIN = /usr/bin
TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(OBJROOT)/wrappers
	$(MY_CC) $(MY_CFLAGS) $(VERSIONERFLAGS) $(VERSIONER_C) -o $(DSTROOT)$(TEMPWRAPPER)
	@set -x && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    ditto $(DSTROOT)$(TEMPWRAPPER) $(DSTROOT)$(MERGEBIN)/$$w || exit 1; \
	done
	rm -f $(DSTROOT)$(TEMPWRAPPER)
	cd $(DSTROOT)$(MERGEBIN) && ls | sort > $(DSTROOT)$(VERSIONBINLIST)

DUMMY = dummy.pl
$(OBJROOT)/wrappers:
	install -d $(DSTROOT)$(MERGEBIN)
	install $(FIX)/$(DUMMY) $(DSTROOT)$(MERGEBIN)
	@set -x && \
	touch $(OBJROOT)/wrappers && \
	for vers in $(ORDEREDVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEBIN) && \
	    for f in *; do \
		if file $$f | head -1 | fgrep -q script; then \
		    fv=`echo $$f | sed -E 's/(\.[^.]*)?$$/'"$$vers&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEBIN)/$$fv && \
		    sed "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv && \
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

versionerdir:
	install -d $(DSTROOT)$(VERSIONERDIR)
	install -m 0644 $(VERSIONER_C) $(DSTROOT)$(VERSIONERDIR)
	install -d $(DSTROOT)$(VERSIONERDIR)/$(Project)/fix
	cd $(FIX) && rsync -pt $(VERSIONERFIX) $(DSTROOT)$(VERSIONERDIR)/$(Project)/fix
	echo DEFAULT = $(DEFAULT) > $(DSTROOT)$(VERSIONVERSIONS)
	for vers in $(KNOWNVERSIONS); do \
	    echo $$vers >> $(DSTROOT)$(VERSIONVERSIONS) || exit 1; \
	done
	@set -x && ( \
	    printf '#define DEFAULTVERSION "%s"\n' $(DEFAULT) && \
	    echo '#define NVERSIONS (sizeof(versions) / sizeof(const char *))' && \
	    echo '#define PROJECT "$(Project)"' && \
	    printf '#define UPROJECT "%s"\n' `echo $(Project) | tr a-z A-Z` && \
	    echo 'static const char *versions[] = {' && \
	    for v in $(sort $(VERSIONS) $(ADDEDVERSIONS)); do \
		printf '    "%s",\n' $$v || exit 1; \
	    done && \
	    echo '};' ) > $(DSTROOT)$(VERSIONHEADER)

rmusrlocal:
	rm -rf $(DSTROOT)/usr/local

rmLibrary:
	rm -rf $(DSTROOT)/Library

mergedefault:
ifneq ($(strip $(MERGEDEFAULT)),)
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)
else
	@true
endif

MERGEMAN = /usr/share/man
mergeman: domergeman customman listman

domergeman:
	@set -x && \
	for vers in $(ORDEREDVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEMAN) && \
	    for d in man*; do \
		cd $$d && \
		for f in `find . -type f -name '*.*' | sed 's,^\./,,'`; do \
		    ff=`echo $$f | sed -E "s/\.[^.]*(.gz)?$$/$$vers&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEMAN)/$$d/$$ff && \
		    if [ ! -e $(DSTROOT)$(MERGEMAN)/$$d/$$f ]; then \
			ln -fs $$ff $(DSTROOT)$(MERGEMAN)/$$d/$$f; \
		    fi || exit 1; \
		done && \
		cd .. || exit 1; \
	    done || exit 1; \
	done

CUSTOMTEMP = .temp.1
customman: $(OBJROOT)/wrappers
	@set -x && \
	sed -e 's/@VERSION_DEFAULT@/$(ENV_VERSION_DEFAULT)/g' \
	    -e 's/@VERSION_ALT@/$(ENV_VERSION_ALT)/g' \
	    $(FIX)/$(Project).1 > $(DSTROOT)$(MERGEMAN)/man1/$(CUSTOMTEMP) && \
	gzip $(DSTROOT)$(MERGEMAN)/man1/$(CUSTOMTEMP) && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    rm -f $(DSTROOT)$(MERGEMAN)/man1/$$w.1.gz && \
	    ln -f $(DSTROOT)$(MERGEMAN)/man1/$(CUSTOMTEMP).gz $(DSTROOT)$(MERGEMAN)/man1/$$w.1.gz || exit 1; \
	done && \
	rm -f $(DSTROOT)$(MERGEMAN)/man1/$(CUSTOMTEMP).gz

listman:
	cd $(DSTROOT)$(MERGEMAN) && find . ! -type d | sed 's,^\./,,' | sort > $(DSTROOT)$(VERSIONMANLIST)

OPENSOURCEVERSIONS = /usr/local/OpenSourceVersions
PLIST = $(OPENSOURCEVERSIONS)/perl.plist
mergeplist:
	mkdir -p $(DSTROOT)$(OPENSOURCEVERSIONS)
	echo '<plist version="1.0">' > $(DSTROOT)$(PLIST)
	echo '<array>' >> $(DSTROOT)$(PLIST)
	@set -x && \
	for vers in $(VERSIONS); do \
	    if grep -q '^<array>' $(OBJROOT)/$$vers/DSTROOT$(PLIST); then \
		sed -e '/^<\/*plist/d' -e '/^<\/*array>/d' $(OBJROOT)/$$vers/DSTROOT$(PLIST) >> $(DSTROOT)$(PLIST); \
	    else \
		sed -e '/^<\/*plist/d' -e 's/^/	/' $(OBJROOT)/$$vers/DSTROOT$(PLIST) >> $(DSTROOT)$(PLIST); \
	    fi || exit 1; \
	done
	echo '</array>' >> $(DSTROOT)$(PLIST)
	echo '</plist>' >> $(DSTROOT)$(PLIST)
	chmod 644 $(DSTROOT)$(PLIST)

MERGEVERSIONS = \
    Library \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done
