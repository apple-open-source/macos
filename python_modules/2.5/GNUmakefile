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

build::
	@set -x && \
	for vers in $(VERSIONS); do \
	    no64= && \
	    if echo $(NO64) | fgrep -q $$vers; then \
		no64=YES; \
	    fi && \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		TOPOBJROOT='$(OBJROOT)' \
		VERSIONER_PYTHON_VERSION=$$vers \
		VERSIONER_PYTHON_PREFER_32_BIT=yes \
		$(MAKE) -f Makefile install Project=$(Project) NO64=$$no64 \
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

#merge: mergebegin mergedefault mergeversions mergebin mergeman
merge: mergebegin mergeversions mergebin mergedefault

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

MERGEDEFAULT = \
    usr/local
mergedefault:
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)

#MYVERSIONMANLIST = $(OBJROOT)/usr-share-man.list
#VERSIONMANLIST = $(VERSIONERDIR)/$(PYTHONPROJECT)/usr-share-man.list
#MERGEMAN = /usr/share/man
#mergeman:
#	@set -x && \
#	for vers in $(ORDEREDVERS); do \
#	    cd $(OBJROOT)/$$vers/DSTROOT$(MERGEMAN) && \
#	    for d in man*; do \
#		cd $$d && \
#		for f in *.gz; do \
#		    ff=`echo $$f | sed "s/\.[^.]*\.gz/$$vers&/"` && \
#		    ditto $$f $(DSTROOT)$(MERGEMAN)/$$d/$$ff && \
#		    if [ ! -e $(DSTROOT)$(MERGEMAN)/$$d/$$f ]; then \
#			ln -fs $$ff $(DSTROOT)$(MERGEMAN)/$$d/$$f; \
#		    fi || exit 1; \
#		done && \
#		cd .. || exit 1; \
#	    done || exit 1; \
#	done
#	cd $(DSTROOT)$(MERGEMAN) && \
#	find . ! -type d | sed 's,^\./,,' | sort > $(MYVERSIONMANLIST) && \
#	rm -fv `comm -12 $(VERSIONMANLIST) $(MYVERSIONMANLIST)`

MERGEVERSIONS = \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done
