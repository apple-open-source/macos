##---------------------------------------------------------------------
# GNUmakefile for CPAN (supporting multiple versions)
##---------------------------------------------------------------------
Project = CPAN
PERLPROJECT = perl
MYFIX = $(SRCROOT)/fix
VERSIONERDIR = /usr/local/versioner
PERLVERSIONS = $(VERSIONERDIR)/$(PERLPROJECT)/versions
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERLVERSIONS))
VERSIONS = 5.10 5.8
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
build::
	@set -x && \
	for vers in $(VERSIONS); do \
	    v=`grep "^$$vers" $(PERLVERSIONS)` && \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		VERSIONER_PERL_VERSION=$$v \
		VERSIONER_PERL_PREFER_32_BIT=yes \
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

merge: mergebegin mergedefault mergeversions mergebin mergeman

mergebegin:
	@echo ####### Merging #######

MERGEBIN = /usr/bin
TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(OBJROOT)/wrappers
	cc $(RC_CFLAGS) $(VERSIONERFLAGS) $(VERSIONERDIR)/versioner.c -o $(DSTROOT)$(TEMPWRAPPER)
	@set -x && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    ln -f $(DSTROOT)$(TEMPWRAPPER) $(DSTROOT)$(MERGEBIN)/$$w || exit 1; \
	done
	rm -f $(DSTROOT)$(TEMPWRAPPER)

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
		    if head -1 $(DSTROOT)$(MERGEBIN)/$$fv | fgrep -q wxPerl; then \
			sed "s/@VERSION@/$$v/g" $(MYFIX)/wxPerl.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv; \
		    else \
			sed "s/@VERSION@/$$v/g" $(FIX)/scriptvers.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv; \
		    fi && \
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
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)

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
