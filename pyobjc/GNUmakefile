##---------------------------------------------------------------------
# Makefile for pyobjc (supporting multiple versions)
##---------------------------------------------------------------------
Project = pyobjc
PYTHONPROJECT = python
MYFIX = $(SRCROOT)/fix
VERSIONERDIR = /usr/local/versioner
PYTHONVERSIONS = $(VERSIONERDIR)/$(PYTHONPROJECT)/versions
INCOMPATIBLE = 3.0
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PYTHONVERSIONS))
KNOWNVERSIONS := $(filter-out $(INCOMPATIBLE), $(shell grep '^[0-9]' $(PYTHONVERSIONS)))
VERSIONS = $(sort $(KNOWNVERSIONS))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(VERSIONERDIR)/$(PYTHONPROJECT) -I$(MYFIX) -framework CoreFoundation
OSV = OpenSourceVersions
OSL = OpenSourceLicenses

FIX = $(VERSIONERDIR)/$(PYTHONPROJECT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

installsrc:
	@echo "*** pyobjc installsrc ***"
	$(MAKE) -f Makefile installsrc Project=$(Project)
	for vers in $(VERSIONS); do \
	    [ ! -d $$vers ] || $(MAKE) -C $$vers -f Makefile installsrc Project=$(Project) SRCROOT="$(SRCROOT)/$$vers" || exit 1; \
	done

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

build::
	$(MKDIR) $(OBJROOT)/$(OSL)
	$(MKDIR) $(OBJROOT)/$(OSV)
	@set -x && \
	for vers in $(VERSIONS); do \
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
		$(MAKE) $$Copt -f Makefile install Project=$(Project) \
		SRCROOT="$$srcroot" \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		OSL='$(OBJROOT)/$(OSL)' OSV='$(OBJROOT)/$(OSV)' \
		RC_ARCHS='$(RC_ARCHS) -g' >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
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
merge: mergebegin mergedefault mergeversions rmempty

mergebegin:
	@echo '####### Merging #######'

#mergebin:
#	@set -x && \
#	for vers in $(ORDEREDVERS); do \
#	    cd $(DSTROOT)/System/Library/Frameworks/Python.framework/Versions/$$vers/Extras/bin && \
#	    for f in *; do \
#		sed -e '/^1a/,/^\./d' -e "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - $$f || exit 1; \
#	    done || exit 1; \
#	done

PYFRAMEWORK = /System/Library/Frameworks/Python.framework
PYFRAMEWORKDOCUMENTATION = $(PYFRAMEWORK)/Documentation
PYFRAMEWORKEXAMPLES = $(PYFRAMEWORK)/Examples
mergedefault:
	install -d $(DSTROOT)$(PYFRAMEWORKDOCUMENTATION) $(DSTROOT)$(PYFRAMEWORKEXAMPLES)
	ditto $(OBJROOT)/$(DEFAULT)/DSTROOT/Developer/Documentation/Python $(DSTROOT)$(PYFRAMEWORKDOCUMENTATION)
	ditto $(OBJROOT)/$(DEFAULT)/DSTROOT/Developer/Examples/Python $(DSTROOT)$(PYFRAMEWORKEXAMPLES)
	find $(DSTROOT)$(PYFRAMEWORKDOCUMENTATION) -size 0 -delete

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

# remove empty files (specific to pyobjc-2.3)
EMPTY = /Developer/Documentation/Python/PyObjC/10PyObjCTools.txt
rmempty:
	@set -x && \
	for empty in $(EMPTY); do \
	    rm -f $(DSTROOT)$$empty || exit 1; \
	done
