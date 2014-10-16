##---------------------------------------------------------------------
# Makefile for python_modules (supporting multiple versions)
##---------------------------------------------------------------------
Project = python_modules
PYTHONPROJECT = python
MYFIX = $(SRCROOT)/fix
_VERSIONERDIR := /usr/local/versioner
# Look for /usr/local/versioner in $(SDKROOT), defaulting to /usr/local/versioner
VERSIONERDIR := $(or $(wildcard $(SDKROOT)$(_VERSIONERDIR)),$(_VERSIONERDIR))
PYTHONVERSIONS = $(VERSIONERDIR)/$(PYTHONPROJECT)/versions
INCOMPATIBLE =
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PYTHONVERSIONS))
KNOWNVERSIONS := $(filter-out $(INCOMPATIBLE), $(shell grep '^[0-9]' $(PYTHONVERSIONS)))
BOOTSTRAPPYTHON =
VERSIONS = $(sort $(KNOWNVERSIONS) $(BOOTSTRAPPYTHON))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(VERSIONERDIR)/$(PYTHONPROJECT) -I$(MYFIX) -framework CoreFoundation
OSV = OpenSourceVersions
OSL = OpenSourceLicenses

FIX = $(VERSIONERDIR)/$(PYTHONPROJECT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')
TMPPREFIX = $(OBJROOT)/Root

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

installsrc: afterinstallsrc

afterinstallsrc:
	for i in $(SRCROOT)/Modules/*; do \
	    [ ! -d $$i ] || $(MAKE) -C $$i afterinstallsrc Project=$(Project) || exit 1; \
	done
	$(MAKE) -C $(SRCROOT)/tmpprefix afterinstallsrc

build::
	$(MKDIR) $(OBJROOT)/$(OSL)
	$(MKDIR) $(OBJROOT)/$(OSV)
	$(MAKE) -C tmpprefix \
	    OSL='$(OBJROOT)/$(OSL)' OSV='$(OBJROOT)/$(OSV)' \
	    TMPPREFIX='$(TMPPREFIX)'
	@set -x && \
	for vers in $(VERSIONS); do \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		PATH="$(TMPPREFIX)/bin:$$PATH" \
		VERSIONER_PYTHON_VERSION=$$vers \
		VERSIONER_PYTHON_PREFER_32_BIT=yes \
		$(MAKE) -f Makefile install Project=$(Project) \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		OSL='$(OBJROOT)/$(OSL)' OSV='$(OBJROOT)/$(OSV)' \
		TMPPREFIX='$(TMPPREFIX)' \
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
