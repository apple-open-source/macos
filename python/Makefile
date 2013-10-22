##---------------------------------------------------------------------
# Makefile for python (supporting multiple versions)
##---------------------------------------------------------------------
Project = python
VERSIONERDIR = /usr/local/versioner
FIX = $(SRCROOT)/fix
DEFAULT = 2.7
VERSIONS = 2.5 2.6 2.7
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
REVERSEVERS := $(filter-out $(DEFAULT),$(VERSIONS)) $(DEFAULT)

PYFRAMEWORK = /System/Library/Frameworks/Python.framework
PYFRAMEWORKVERSIONS = $(PYFRAMEWORK)/Versions
VERSIONERFLAGS = -std=gnu99 -Wall -mdynamic-no-pic -I$(DSTROOT)$(VERSIONERDIR)/$(Project) -I$(FIX) -framework CoreFoundation

RSYNC = rsync -rlpt
PWD = $(shell pwd)

ifeq ($(MAKECMDGOALS),)
MAKECMDGOALS = build
endif
ifneq ($(filter build install,$(MAKECMDGOALS)),)
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
endif

ifndef SRCROOT
export SRCROOT = $(PWD)
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
##---------------------------------------------------------------------
# Before, we used the versioned gcc (e.g., gcc-4.2) because newer compiler
# would occasionally be incompatible with the compiler flags that python
# records.  With clang, it doesn't use names with versions, so we just go
# back to using plain cc and c++.  With 11952207, we will automatically
# get xcrun support.
##---------------------------------------------------------------------
export MY_CC = cc
export MY_CXX = c++

##---------------------------------------------------------------------
# The "strip" perl script, works around a verification error caused by a
# UFS bug (stripping a multi-link file breaks the link, and sometimes causes
# the wrong file to be stripped/unstripped).  By using the "strip" perl script,
# it not only causes the correct file to be stripped, but also preserves the
# link.
#
# The cc/c++ scripts take a -no64 argument, which causes 64-bit architectures
# to be removed, before calling the real compiler.
##---------------------------------------------------------------------
export PATH:=$(OBJROOT)/bin:$(PATH)

TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

VERSIONVERSIONS = $(VERSIONERDIR)/$(Project)/versions
VERSIONHEADER = $(VERSIONERDIR)/$(Project)/versions.h
VERSIONBINLIST = $(VERSIONERDIR)/$(Project)/usr-bin.list
VERSIONMANLIST = $(VERSIONERDIR)/$(Project)/usr-share-man.list
VERSIONERFIX = dummy.py scriptvers.ed
build::
	$(RSYNC) '$(SRCROOT)/' '$(OBJROOT)'
	ln -sf _no64 $(OBJROOT)/bin/$(MY_CC)
	ln -sf _no64 $(OBJROOT)/bin/$(MY_CXX)
	@set -x && \
	for vers in $(VERSIONS); do \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" && \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		TOPSRCROOT='$(SRCROOT)' \
		$(MAKE) -C "$(OBJROOT)/$$vers" install \
		SRCROOT="$(SRCROOT)/$$vers" \
		OBJROOT="$(OBJROOT)/$$vers" \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" \
		RC_ARCHS='$(RC_ARCHS)' >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		touch "$(OBJROOT)/$$vers/.ok" && \
		echo "######## Finished $$vers:" `date` '########' >> "$(SYMROOT)/$$vers/LOG" 2>&1 \
	    ) & \
	done && \
	wait && \
	install -d $(DSTROOT)$(VERSIONERDIR)/$(Project)/fix && \
	(cd $(FIX) && rsync -pt $(VERSIONERFIX) $(DSTROOT)$(VERSIONERDIR)/$(Project)/fix) && \
	echo DEFAULT = $(DEFAULT) > $(DSTROOT)$(VERSIONVERSIONS) && \
	for vers in $(VERSIONS); do \
	    echo $$vers >> $(DSTROOT)$(VERSIONVERSIONS) && \
	    cat $(SYMROOT)/$$vers/LOG && \
	    rm -f $(SYMROOT)/$$vers/LOG || exit 1; \
	done && \
	if [ $(TESTOK) ]; then \
	    $(MAKE) merge; \
	else \
	    echo '#### error detected, not merging'; \
	    exit 1; \
	fi

##---------------------------------------------------------------------
# After the rest of the merge, we need to merge /usr/lib manually.  The
# problem is that now libpythonN.dylib is ambiguous, and probably best
# to just avoid it, and just keep libpythonN.M.dylib.  libpython.dylib
# will correspond with the default version.
##---------------------------------------------------------------------
MERGELIB = /usr/lib
merge: mergebegin mergedefault mergeversions mergeplist mergebin mergeman fixsmptd
	install -d $(DSTROOT)$(MERGELIB)
	@set -x && \
	for vers in $(VERSIONS); do \
	    ln -sf ../..$(PYFRAMEWORKVERSIONS)/$$vers/Python $(DSTROOT)$(MERGELIB)/lib$(Project)$$vers.dylib && \
	    ln -sf ../..$(PYFRAMEWORKVERSIONS)/$$vers/lib/$(Project)$$vers $(DSTROOT)$(MERGELIB)/$(Project)$$vers || exit 1; \
	done
	ln -sf lib$(Project)$(DEFAULT).dylib $(DSTROOT)$(MERGELIB)/lib$(Project).dylib

mergebegin:
	@echo ####### Merging #######

MERGEBIN = /usr/bin
TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(DSTROOT)$(VERSIONHEADER) $(OBJROOT)/wrappers
	cc $(RC_CFLAGS) $(VERSIONERFLAGS) $(VERSIONERDIR)/versioner.c -o $(DSTROOT)$(TEMPWRAPPER)
	@set -x && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    ln -f $(DSTROOT)$(TEMPWRAPPER) $(DSTROOT)$(MERGEBIN)/$$w || exit 1; \
	done
	rm -f $(DSTROOT)$(TEMPWRAPPER)
	cd $(DSTROOT)$(MERGEBIN) && ls | sort > $(DSTROOT)$(VERSIONBINLIST)

DUMMY = dummy.py
$(OBJROOT)/wrappers:
	install -d $(DSTROOT)$(MERGEBIN)
	install $(FIX)/$(DUMMY) $(DSTROOT)$(MERGEBIN)
	@set -x && \
	touch $(OBJROOT)/wrappers && \
	for vers in $(ORDEREDVERS); do \
	    pbin=$(PYFRAMEWORKVERSIONS)/$$vers/bin && \
	    cd $(DSTROOT)$$pbin && \
	    if [ -e 2to3 ]; then \
		mv 2to3 2to3$$vers && \
		ln -s 2to3$$vers 2to3 && \
		sed -e 's/@SEP@//g' -e "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - 2to3$$vers; \
	    fi && \
	    for f in `find . -type f | sed 's,^\./,,'`; do \
		f0=`echo $$f | sed "s/$$vers//"` && \
		ln -sf ../..$$pbin/$$f $(DSTROOT)$(MERGEBIN)/$$f && \
		if file $$f | head -1 | fgrep -q script; then \
		    sed -e 's/@SEP@//g' -e "s/@VERSION@/$$vers/g" $(FIX)/scriptvers.ed | ed - $$f && \
		    if [ ! -e $(DSTROOT)$(MERGEBIN)/$$f0 ]; then \
			ln -f $(DSTROOT)$(MERGEBIN)/$(DUMMY) $(DSTROOT)$(MERGEBIN)/$$f0; \
		    fi; \
		else \
		    echo $$f0 >> $@; \
		fi || exit 1; \
	    done || exit 1; \
	done
	rm -f $(DSTROOT)$(MERGEBIN)/$(DUMMY)

$(DSTROOT)$(VERSIONHEADER):
	@set -x && ( \
	    echo '#define DEFAULTVERSION "$(DEFAULT)"' && \
	    echo '#define NVERSIONS (sizeof(versions) / sizeof(const char *))' && \
	    echo '#define PROJECT "$(Project)"' && \
	    printf '#define UPROJECT "%s"\n' `echo $(Project) | tr a-z A-Z` && \
	    echo 'static const char *versions[] = {' && \
	    touch $(OBJROOT)/versions && \
	    for vers in $(VERSIONS); do \
		echo $$vers >> $(OBJROOT)/versions || exit 1; \
	    done && \
	    for vers in `sort -u $(OBJROOT)/versions`; do \
		printf '    "%s",\n' $$vers || exit 1; \
	    done && \
	    echo '};' ) > $@

MERGEDEFAULT = \
    usr/local/OpenSourceLicenses
mergedefault:
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)

MERGEMAN = /usr/share/man
mergeman: domergeman customman listman

# When merging man pages from the multiple versions, allow the man pages
# to be compressed (.gz suffix) or not.
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

# When adding custom python.1 and pythonw.1 man pages, autodetect if we are
# compressing man pages, and if so, compress these custom man pages as well
CUSTOMTEMP = .temp.1
customman: $(OBJROOT)/wrappers
	@set -x && \
	cp -f $(FIX)/$(Project).1 $(DSTROOT)$(MERGEMAN)/man1/$(CUSTOMTEMP) && \
	cd $(DSTROOT)$(MERGEMAN)/man1 && \
	suffix='' && \
	if ls | grep -q '\.gz$$'; then suffix='.gz'; fi && \
	if [ "$${suffix}" ]; then gzip $(CUSTOMTEMP); fi && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    rm -f $${w}.1$${suffix} && \
	    ln -f $(CUSTOMTEMP)$${suffix} $${w}.1$${suffix} || exit 1; \
	done && \
	rm -f $(CUSTOMTEMP)$${suffix}

listman:
	cd $(DSTROOT)$(MERGEMAN) && find . ! -type d | sed 's,^\./,,' | sort > $(DSTROOT)$(VERSIONMANLIST)

OPENSOURCEVERSIONS = /usr/local/OpenSourceVersions
PLIST = $(OPENSOURCEVERSIONS)/$(Project).plist
mergeplist:
	mkdir -p $(DSTROOT)/$(OPENSOURCEVERSIONS)
	echo '<plist version="1.0">' > $(DSTROOT)/$(PLIST)
	echo '<array>' >> $(DSTROOT)/$(PLIST)
	@set -x && \
	for vers in $(VERSIONS); do \
	    sed -e '/^<\/*plist/d' -e '/^<\/*array/d' -e 's/^/	/' $(OBJROOT)/$$vers/DSTROOT/$(PLIST) >> $(DSTROOT)/$(PLIST) || exit 1; \
	done
	echo '</array>' >> $(DSTROOT)/$(PLIST)
	echo '</plist>' >> $(DSTROOT)/$(PLIST)
	chmod 644 $(DSTROOT)/$(PLIST)

MERGEVERSIONSCONDITIONAL = \
    Developer/Applications
MERGEVERSIONS = \
    Library \
    usr/include
MERGEREVERSEVERSIONS = \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) && \
	    for c in $(MERGEVERSIONSCONDITIONAL); do \
		if [ -e "$$c" ]; then \
		    rsync -Ra "$$c" $(DSTROOT); \
		fi || exit 1; \
	    done || exit 1; \
	done
	for vers in $(REVERSEVERS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEREVERSEVERSIONS) $(DSTROOT) || exit 1; \
	done

fixsmptd:
	set -x && \
	cd $(DSTROOT)/usr/bin && \
	mv -f smtpd.py smtpd.py.bak && \
	cp -pf smtpd.py.bak smtpd.py && \
	rm -f smtpd.py.bak && \
	for i in smtpd*.py; do \
	    ed - $$i < $(FIX)/smtpd.py.ed || exit 1; \
	done
