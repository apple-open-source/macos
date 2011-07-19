##---------------------------------------------------------------------
# GNUmakefile for CPAN (supporting multiple versions)
##---------------------------------------------------------------------
Project = CPAN
PERLPROJECT = perl
MYFIX = $(SRCROOT)/fix
VERSIONERDIR = /usr/local/versioner
PERLVERSIONS = $(VERSIONERDIR)/$(PERLPROJECT)/versions
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERLVERSIONS))
VERSIONS := $(shell grep -v '^DEFAULT' $(PERLVERSIONS))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(VERSIONERDIR)/$(PERLPROJECT) -I$(MYFIX) -framework CoreFoundation

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
export RC_ProjectName = $(Project)
endif

FIX = $(VERSIONERDIR)/$(PERLPROJECT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

MYVERSIONBINLIST = $(OBJROOT)/usr-bin.list
MYVERSIONMANLIST = $(OBJROOT)/usr-share-man.list
VERSIONBINLIST = $(VERSIONERDIR)/$(PERLPROJECT)/usr-bin.list
VERSIONMANLIST = $(VERSIONERDIR)/$(PERLPROJECT)/usr-share-man.list
PERL_CC = $(shell perl -MConfig -e 'print $$Config::Config{cc}')
build:: $(foreach v,$(VERSIONS),$(SRCROOT)/$(v).inc)
	@set -x && \
	for vers in $(VERSIONS); do \
	    v=`grep "^$$vers" $(PERLVERSIONS)` && \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		VERSIONER_PERL_VERSION=$$v \
		VERSIONER_PERL_PREFER_32_BIT=yes \
		CC=$(PERL_CC) \
		$(MAKE) -f Makefile install \
		VERS=$$vers \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		RC_ARCHS='$(RC_ARCHS)' >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
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

merge: mergebegin mergedefault mergeversions mergeoss
ifeq ($(shell [ -d $(OBJROOT)/$(DEFAULT)/DSTROOT$(MERGEBIN) ] && echo YES || echo NO),YES)
merge: mergebin
endif
merge: mergeman

mergebegin:
	@echo '####### Merging #######'

TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(OBJROOT)/wrappers
	@[ ! -s $(OBJROOT)/wrappers ] || { set -x && \
	cc $(RC_CFLAGS) $(VERSIONERFLAGS) $(VERSIONERDIR)/versioner.c -o $(DSTROOT)$(TEMPWRAPPER) && \
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
	    v=`grep "^$$vers" $(PERLVERSIONS)` && \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEBIN) && \
	    for f in *; do \
		if file $$f | head -1 | fgrep -q script; then \
		    fv=`echo $$f | sed -E 's/(\.[^.]*)?$$/'"$$v&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEBIN)/$$fv && \
		    sed "s/@VERSION@/$$v/g" $(FIX)/scriptvers.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv && \
		    if [ ! -e $(DSTROOT)$(MERGEBIN)/$$f ]; then \
			ln -f $(DSTROOT)$(MERGEBIN)/$(DUMMY) $(DSTROOT)$(MERGEBIN)/$$f; \
		    fi; \
		elif echo $$f | grep -q "[^0-9]$$v"; then \
		    true; \
		else \
		    echo $$f >> $(OBJROOT)/wrappers && \
		    if [ -e $$f$$v ]; then \
			ditto $$f$$v $(DSTROOT)$(MERGEBIN)/$$f$$v; \
		    else \
			ditto $$f $(DSTROOT)$(MERGEBIN)/$$f$$v; \
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
	    v=`grep "^$$vers" $(PERLVERSIONS)` && \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEMAN) && \
	    for d in man*; do \
		cd $$d && \
		for f in *.gz; do \
		    ff=`echo $$f | sed "s/\.[^.]*\.gz/$$v&/"` && \
		    ditto $$f $(DSTROOT)$(MERGEMAN)/$$d/$$ff && \
		    if [ ! -e $(DSTROOT)$(MERGEMAN)/$$d/$$f ]; then \
			ln -fs $$ff $(DSTROOT)$(MERGEMAN)/$$d/$$f; \
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
