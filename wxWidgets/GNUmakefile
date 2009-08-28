##---------------------------------------------------------------------
# GNUmakefile for wxWidgets
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory.  The wxPythonSrc
# tarball contains both wxWidgets and wxPython
##---------------------------------------------------------------------
PROJECT = wxWidgets
NAME = wxPython-src
VERSION = 2.8.8.1
export CURRENT_VERSION = $(shell echo $(VERSION) | sed 's/\.[^.]*$$//')
export BASE_VERSION = $(shell echo $(CURRENT_VERSION) | sed 's/\.[^.]*$$//')
export COMPATIBILITY_VERSION = 2.6
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.bz2
FAKEBIN = $(OBJROOT)/bin
WXWIDGETSTOP = $(OBJROOT)/$(PROJECT)
WXWIDGETSBUILD = $(WXWIDGETSTOP)/darwin

PYTHONPROJECT = python
VERSIONERDIR = /usr/local/versioner
PYTHONVERSIONS = $(VERSIONERDIR)/$(PYTHONPROJECT)/versions
INCOMPATIBLE = 3.0
DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PYTHONVERSIONS))
VERSIONS := $(filter-out $(INCOMPATIBLE), $(shell grep '^[0-9]' $(PYTHONVERSIONS)))
ORDEREDVERS := $(DEFAULT) $(filter-out $(DEFAULT),$(VERSIONS))
TESTOK := -f $(shell echo $(foreach vers,$(VERSIONS),$(OBJROOT)/$(vers)/.ok) | sed 's/ / -a -f /g')

WXPYTHONPROJECT = wxPython
WXPYTHONPROJECTVERS = $(WXPYTHONPROJECT)-$(VERSION)
WXPYTHONBUILD = $(OBJROOT)/$(PROJECT)/$(WXPYTHONPROJECT)
WXPYTHONDEFAULT = $(OBJROOT)/$(DEFAULT)

DOC = $(DSTROOT)/Developer/Documentation
DOCPYTHON = $(DOC)/Python
DOCPYTHONWXPYTHON = $(DOCPYTHON)/$(WXPYTHONPROJECT)
DOCWXWIDGETS = $(DOC)/$(PROJECT)
DOCWXWIDGETSWXPYTHON = $(DOCWXWIDGETS)/$(WXPYTHONPROJECT)
EXAMPLES = $(DSTROOT)/Developer/Examples
EXAMPLESPYTHON = $(EXAMPLES)/Python
EXAMPLESPYTHONWXPYTHON = $(EXAMPLESPYTHON)/$(WXPYTHONPROJECT)
EXAMPLESWXWIDGETS = $(EXAMPLES)/$(PROJECT)
EXAMPLESWXWIDGETSWXPYTHON = $(EXAMPLESWXWIDGETS)/$(WXPYTHONPROJECT)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

ifndef LD_TWOLEVEL_NAMESPACE
export LD_TWOLEVEL_NAMESPACE = YES
endif
ifndef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET = 10.5
endif

no_target: $(PROJECT) $(WXPYTHONPROJECT)

$(PROJECT): $(WXWIDGETSBUILD)
	$(MAKE) -C $(OBJROOT) -f Makefile \
	    SRCROOT=$(OBJROOT) OBJROOT=$(WXWIDGETSBUILD)

$(WXPYTHONPROJECT): $(WXWIDGETSBUILD) $(FAKEBIN)/wx-config
	$(MAKE) -C $(OBJROOT) -f Makefile.wxPython \
	    OBJROOT=$(WXPYTHONBUILD) FAKEBIN=$(FAKEBIN)

$(FAKEBIN)/wx-config:
	mkdir -p $(FAKEBIN)
	cp $(DSTROOT)/usr/bin/wx-config $(FAKEBIN)/wx-config
	sed 's,XXXDSTROOTXXX,$(DSTROOT),' $(SRCROOT)/fix/wx-config.ed | ed - $(FAKEBIN)/wx-config 

$(WXWIDGETSBUILD):
	rsync -a $(SRCROOT)/ $(OBJROOT)
	@set -x && \
	cd $(OBJROOT) && \
	gnutar xjf $(TARBALL) && \
	rm -rf $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	chmod a-x $(PROJECT)/wxPython/demo/bmp_source/customcontrol.png && \
	ed - $(PROJECT)/configure < fix/configure.ed && \
	ed - $(PROJECT)/Makefile.in < fix/Makefile.in.ed && \
	ed - $(PROJECT)/include/wx/platform.h < fix/platform.h.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.c < fix/MoreFilesX.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.h < fix/MoreFilesX.ed && \
	ex - $(PROJECT)/wxPython/config.py < fix/O3.ex && \
	cp -f fix/anykey.wav $(PROJECT)/wxPython/demo/data/anykey.wav && \
	for i in `find $(PROJECT) -name Makefile.in | xargs fgrep -l -e -compatibility_version`; do \
	    ed - $$i < fix/compatibility_version.ed || exit 1; \
	done
	find $(OBJROOT) -type f -perm +111 \( -name '*.h' -o -name '*.cpp' -o -name '*.icns' -o -name '*.png' -o -name '*.txt' \) | xargs chmod -v a-x
	mkdir $(WXWIDGETSBUILD)

install: $(PROJECT)install $(WXPYTHONPROJECT)install
	install -d $(OSV)
	install $(SRCROOT)/$(PROJECT).plist $(OSV)
	install -d $(OSL)
	install $(OBJROOT)/$(PROJECT)/docs/licence.txt $(OSL)/$(PROJECT).txt

$(PROJECT)install: $(WXWIDGETSBUILD)
	$(MAKE) -C $(OBJROOT) -f Makefile install \
	    SRCROOT=$(OBJROOT) OBJROOT=$(WXWIDGETSBUILD)
	rm -rf $(DSTROOT)/usr/share/locale
	install -d -g admin -m 0775 $(DOCWXWIDGETS)
	rsync -rlt $(WXWIDGETSTOP)/docs $(DOCWXWIDGETS)
	-chown -R root:admin $(DOCWXWIDGETS)
	-chmod -R g+w $(DOCWXWIDGETS)
	strip $(DSTROOT)/usr/bin/wxrc-2.8
	rm -f $(DSTROOT)/Developer/Documentation/wxWidgets/docs/html/wx/wx.cn1

$(WXPYTHONPROJECT)install: $(WXWIDGETSBUILD) $(FAKEBIN)/wx-config
	@set -x && \
	mv -f '$(WXPYTHONBUILD)' '$(OBJROOT)/$(DEFAULT)' && \
	for vers in $(filter-out $(DEFAULT),$(VERSIONS)); do \
	    ditto '$(OBJROOT)/$(DEFAULT)' "$(OBJROOT)/$$vers" || exit 1; \
	done && \
	for vers in $(VERSIONS); do \
	    mkdir -p "$(SYMROOT)/$$vers" && \
	    mkdir -p "$(OBJROOT)/$$vers/DSTROOT" || exit 1; \
	    (echo "######## Building $$vers:" `date` '########' > "$(SYMROOT)/$$vers/LOG" 2>&1 && \
		TOPSRCROOT='$(SRCROOT)' \
		VERSIONER_PYTHON_VERSION=$$vers \
		VERSIONER_PYTHON_PREFER_32_BIT=yes \
		$(MAKE) -C $(OBJROOT) -f Makefile.wxPython install \
		OBJROOT="$(OBJROOT)/$$vers" FAKEBIN=$(FAKEBIN) \
		DSTROOT="$(OBJROOT)/$$vers/DSTROOT" \
		SYMROOT="$(SYMROOT)/$$vers" >> "$(SYMROOT)/$$vers/LOG" 2>&1 && \
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

merge: mergebegin mergeversions mergefinish

mergebegin:
	@echo '####### Merging #######'

# Normally, the versioned DSTROOT directories should each have /usr/include,
# but setup.py has been hacked to install into "wx-config --prefix"
# (= $(DSTROOT)/usr).  This means the multiple versions are overwriting each
# other.  The hack is in wx/build/config.py, but it isn't clear how to get
# it to do the right thing, so we just leave it for now.
#MERGEDEFAULT = \
#    usr
#mergedefault:
#	cd $(OBJROOT)/$(DEFAULT)/DSTROOT && rsync -Ra $(MERGEDEFAULT) $(DSTROOT)

MERGEVERSIONS = \
    System
mergeversions:
	@set -x && \
	for vers in $(VERSIONS); do \
	    cd $(OBJROOT)/$$vers/DSTROOT && \
	    rsync -Ra $(MERGEVERSIONS) $(DSTROOT) || exit 1; \
	done

mergefinish:
	install -d -g admin -m 0775 $(EXAMPLESWXWIDGETSWXPYTHON)
	rsync -rlt $(WXPYTHONDEFAULT)/demo $(WXPYTHONDEFAULT)/samples $(EXAMPLESWXWIDGETSWXPYTHON)
	-chown -R root:admin $(EXAMPLESWXWIDGETSWXPYTHON)
	-chmod -R g+w $(EXAMPLESWXWIDGETSWXPYTHON)
	install -d -g admin -m 0775 $(EXAMPLESPYTHON)
	ln -s ../$(PROJECT)/$(WXPYTHONPROJECT) $(EXAMPLESPYTHONWXPYTHON)
	install -d -g admin -m 0775 $(DOCWXWIDGETSWXPYTHON)
	rsync -rlt $(WXPYTHONDEFAULT)/docs/ $(DOCWXWIDGETSWXPYTHON)
	-chown -R root:admin $(DOCWXWIDGETSWXPYTHON)
	-chmod -R g+w $(DOCWXWIDGETSWXPYTHON)
	install -d -g admin -m 0775 $(DOCPYTHON)
	ln -s ../$(PROJECT)/$(WXPYTHONPROJECT) $(DOCPYTHONWXPYTHON)
	for i in `find $(EXAMPLESWXWIDGETSWXPYTHON) -name __init__.py -size 0c`; do \
	    echo $$i && \
	    echo '#' > $$i || exit 1; \
	done

.DEFAULT:
	@$(MAKE) -f Makefile $@
