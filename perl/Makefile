##---------------------------------------------------------------------
# Makefile for perl (supporting multiple versions)
##---------------------------------------------------------------------
Project = perl
VERSIONERDIR = /usr/local/versioner
DEFAULT = 5.12
VERSIONS = 5.10 5.12
OTHERVERSIONS = $(filter-out $(DEFAULT),$(VERSIONS))
ORDEREDVERS := $(DEFAULT) $(OTHERVERSIONS)
VERSIONERFLAGS = -std=gnu99 -Wall -DFORCE_TWO_NUMBER_VERSIONS -I$(DSTROOT)$(VERSIONERDIR)/$(Project) -I$(SRCROOT)/versioner -framework CoreFoundation

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
##---------------------------------------------------------------------
# 6320578 - when the default gcc compiler version changes, the recorded
# compiler options for compiler perl was originally built with may be
# incompatible.  So we force perl to use a version-specific name for the
# compiler, which will be recorded, and used in all subsequent extension
# (and other) builds.
#
# 7215115 - for now, we assume cc is a symbolic link to the version-specific
# name of the compiler.  If/when this changes, this fix will have to be redone.
##---------------------------------------------------------------------
export WITH_GCC = $(shell test -L /usr/bin/cc && basename `readlink /usr/bin/cc` || gcc-`gcc -dumpversion | sed -e 's/\([0-9]\{1,\}\.[0-9]\{1,\}\).*/\1/'`)

# define environment variables for each the default and alternate versions
# (ENV_VERSION_DEFAULT and ENV_VERSION_ALT).  Only 2 versions are currently supported.
define VERSION_template
 export ENV_VERSION$(if $(filter $(DEFAULT),$(1)),_DEFAULT,_ALT) = $(1)
endef
$(foreach vers,$(VERSIONS),$(eval $(call VERSION_template,$(vers))))

ARCHFLAGS := $(shell perl -e 'printf "-arch %s\n", join(" -arch ", split(" ", $$ENV{RC_ARCHS}));')

FIX = $(SRCROOT)/fix
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

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
		RC_ARCHS='$(RC_ARCHS)' \
		PERLVERSION=$$vers >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
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

installsrc: custominstallsrc

custominstallsrc:
	@set -x && \
	for vers in $(VERSIONS); do \
	    make -C "$(SRCROOT)/$$vers" custominstallsrc SRCROOT="$(SRCROOT)/$$vers" || exit 1; \
	done

merge: mergebegin mergedefault mergeversions mergeplist mergebin mergeman

mergebegin:
	@echo ####### Merging #######

MERGEBIN = /usr/bin
TEMPWRAPPER = $(MERGEBIN)/.versioner
mergebin: $(DSTROOT)$(VERSIONHEADER) $(OBJROOT)/wrappers
	install $(SRCROOT)/versioner/versioner.c $(DSTROOT)$(VERSIONERDIR)
	cc $(RC_CFLAGS) $(VERSIONERFLAGS) $(SRCROOT)/versioner/versioner.c -o $(DSTROOT)$(TEMPWRAPPER)
	@set -x && \
	for w in `sort -u $(OBJROOT)/wrappers`; do \
	    ln -f $(DSTROOT)$(TEMPWRAPPER) $(DSTROOT)$(MERGEBIN)/$$w || exit 1; \
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

$(DSTROOT)$(VERSIONHEADER):
	@set -x && ( \
	    printf '#define DEFAULTVERSION "%s"\n' $(DEFAULT) && \
	    echo '#define NVERSIONS (sizeof(versions) / sizeof(const char *))' && \
	    echo '#define PROJECT "$(Project)"' && \
	    printf '#define UPROJECT "%s"\n' `echo $(Project) | tr a-z A-Z` && \
	    echo 'static const char *versions[] = {' && \
	    touch $(OBJROOT)/versions && \
	    for v in $(VERSIONS); do \
		echo $$v >> $(OBJROOT)/versions || exit 1; \
	    done && \
	    for v in `sort -u $(OBJROOT)/versions`; do \
		printf '    "%s",\n' $$v || exit 1; \
	    done && \
	    echo '};' ) > $@

MERGEDEFAULT = \
    usr/local/OpenSourceLicenses
mergedefault:
	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)

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
	mkdir -p $(DSTROOT)/$(OPENSOURCEVERSIONS)
	echo '<plist version="1.0">' > $(DSTROOT)/$(PLIST)
	echo '<array>' >> $(DSTROOT)/$(PLIST)
	@set -x && \
	for vers in $(VERSIONS); do \
	    sed -e '/^<\/*plist/d' -e 's/^/	/' $(OBJROOT)/$$vers/DSTROOT/$(PLIST) >> $(DSTROOT)/$(PLIST) || exit 1; \
	done
	echo '</array>' >> $(DSTROOT)/$(PLIST)
	echo '</plist>' >> $(DSTROOT)/$(PLIST)
	chmod 644 $(DSTROOT)/$(PLIST)

MERGEVERSIONS = \
    Library \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done
