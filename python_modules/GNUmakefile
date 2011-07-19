##---------------------------------------------------------------------
# Makefile for python_modules (supporting multiple versions)
##---------------------------------------------------------------------
Project = python_modules
PYTHONPROJECT = python
MYFIX = $(SRCROOT)/fix
VERSIONERDIR = /usr/local/versioner
PYTHONVERSIONS = $(VERSIONERDIR)/$(PYTHONPROJECT)/versions
INCOMPATIBLE = 3.0
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PYTHONVERSIONS))
VERSIONS := $(filter-out $(INCOMPATIBLE), $(shell grep '^[0-9]' $(PYTHONVERSIONS)))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(VERSIONERDIR)/$(PYTHONPROJECT) -I$(MYFIX) -framework CoreFoundation
NO64 = 2.5
OSV = OpenSourceVersions
OSL = OpenSourceLicenses

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

FIX = $(VERSIONERDIR)/$(PYTHONPROJECT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

installsrc: afterinstallsrc

afterinstallsrc:
	$(MAKE) -f Makefile afterinstallsrc Project=$(Project)
	for vers in $(VERSIONS); do \
	    [ ! -d $$vers ] || $(MAKE) -C $$vers -f Makefile afterinstallsrc Project=$(Project) SRCROOT="$(SRCROOT)/$$vers" || exit 1; \
	done

build::
	$(MKDIR) $(OBJROOT)/$(OSL)
	$(MKDIR) $(OBJROOT)/$(OSV)
	@set -x && \
	for vers in $(VERSIONS); do \
	    no64= && \
	    for n in $(NO64); do \
		if [ $$n = $$vers ]; then \
		    no64=YES; \
		    break; \
		fi; \
	    done && \
	    Copt= && \
	    srcroot='$(SRCROOT)' && \
	    if [ -d $$vers ]; then \
		srcroot="$(SRCROOT)/$$vers"; \
		Copt="-C $$vers"; \
	    fi && \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		VERSIONER_PYTHON_VERSION=$$vers \
		VERSIONER_PYTHON_PREFER_32_BIT=yes \
		$(MAKE) $$Copt -f Makefile install Project=$(Project) NO64=$$no64 \
		SRCROOT="$$srcroot" \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		OSL='$(OBJROOT)/$(OSL)' OSV='$(OBJROOT)/$(OSV)' \
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
	$(MKDIR) $(DSTROOT)/usr/local/$(OSL)
	@set -x && \
	cd $(OBJROOT)/$(OSL) && \
	for i in *; do \
	    echo '##########' `basename $$i` '##########' && \
	    cat $$i || exit 1; \
	done > $(DSTROOT)/usr/local/$(OSL)/$(Project).txt
	$(MKDIR) $(DSTROOT)/usr/local/$(OSV)
	(cd $(OBJROOT)/$(OSV) && \
	echo '<plist version="1.0">' && \
	echo '<array>' && \
	cat * && \
	echo '</array>' && \
	echo '</plist>') > $(DSTROOT)/usr/local/$(OSV)/$(Project).plist

#merge: mergebegin mergedefault mergeversions mergebin mergeman
merge: mergebegin mergeversions mergebin mergeman

mergebegin:
	@echo ####### Merging #######

MERGEBIN = /usr/bin
DUMMY = dummy.py
mergebin:
	mkdir -p $(DSTROOT)$(MERGEBIN)
	install $(FIX)/$(DUMMY) $(DSTROOT)$(MERGEBIN)
	@set -x && \
	for vers in $(ORDEREDVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEBIN) && \
	    for f in `find . -type f ! -name "*$$vers*" | sed 's,^\./,,'`; do \
		fv=$$f-$$vers && \
		ditto $$f $(DSTROOT)$(MERGEBIN)/$$fv && \
		sed -e 's/@SEP@/-/g' -e "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - $(DSTROOT)$(MERGEBIN)/$$fv && \
		if [ ! -e $(DSTROOT)$(MERGEBIN)/$$f ]; then \
		    ln -f $(DSTROOT)$(MERGEBIN)/$(DUMMY) $(DSTROOT)$(MERGEBIN)/$$f; \
		fi || exit 1; \
	    done && \
	    cd $(DSTROOT)/System/Library/Frameworks/Python.framework/Versions/$$vers/Extras/bin && \
	    for f in *; do \
		sed -e '/^1a/,/^\./d' -e "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - $$f || exit 1; \
	    done || exit 1; \
	done
	rm -f $(DSTROOT)$(MERGEBIN)/$(DUMMY)

MERGEVERSIONS = \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done

MERGEMAN = usr/share/man
mergeman:
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEMAN) $(DSTROOT)
