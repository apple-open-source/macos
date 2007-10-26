##---------------------------------------------------------------------
# GNUmakefile for wxWidgets
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory.  The wxPythonSrc
# tarball contains both wxWidgets and wxPython
##---------------------------------------------------------------------
PROJECT = wxWidgets
NAME = wxPython-src
VERSION = 2.8.4.0
export CURRENT_VERSION = $(shell echo $(VERSION) | sed 's/\.[^.]*$$//')
export BASE_VERSION = $(shell echo $(CURRENT_VERSION) | sed 's/\.[^.]*$$//')
export COMPATIBILITY_VERSION = 2.6
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.bz2
FAKEBIN = $(OBJROOT)/bin
WXWIDGETSTOP = $(OBJROOT)/$(PROJECT)
WXWIDGETSBUILD = $(WXWIDGETSTOP)/darwin

WXPYTHONPROJECT = wxPython
WXPYTHONPROJECTVERS = $(WXPYTHONPROJECT)-$(VERSION)
WXPYTHONBUILD = $(OBJROOT)/$(PROJECT)/$(WXPYTHONPROJECT)

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

MAKEFILE_IN = \
	contrib/src/deprecated/Makefile.in \
	contrib/src/fl/Makefile.in \
	contrib/src/foldbar/Makefile.in \
	contrib/src/gizmos/Makefile.in \
	contrib/src/mmedia/Makefile.in \
	contrib/src/net/Makefile.in \
	contrib/src/ogl/Makefile.in \
	contrib/src/plot/Makefile.in \
	contrib/src/stc/Makefile.in \
	contrib/src/svg/Makefile.in \
	Makefile.in

$(WXWIDGETSBUILD):
	rsync -a $(SRCROOT)/ $(OBJROOT)
	@set -x && \
	cd $(OBJROOT) && \
	gnutar xjf $(TARBALL) && \
	rm -rf $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	ed - $(PROJECT)/configure < fix/configure.ed && \
	ed - $(PROJECT)/Makefile.in < fix/Makefile.in.ed && \
	ed - $(PROJECT)/include/wx/defs.h < fix/defs.h.ed && \
	ed - $(PROJECT)/include/wx/platform.h < fix/platform.h.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.c < fix/MoreFilesX.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.h < fix/MoreFilesX.ed && \
	ed - $(PROJECT)/src/unix/dlunix.cpp < fix/dlunix.cpp.ed && \
	ex - $(PROJECT)/wxPython/config.py < fix/O3.ex && \
	cp -f fix/anykey.wav $(PROJECT)/wxPython/demo/data/anykey.wav && \
	for i in $(MAKEFILE_IN); do \
	    ed - $(PROJECT)/$$i < fix/compatibility_version.ed; \
	done
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
	$(MAKE) -C $(OBJROOT) -f Makefile.wxPython install \
	    OBJROOT=$(WXPYTHONBUILD) FAKEBIN=$(FAKEBIN)
	install -d -g admin -m 0775 $(EXAMPLESWXWIDGETSWXPYTHON)
	rsync -rlt $(WXPYTHONBUILD)/demo $(WXPYTHONBUILD)/samples $(EXAMPLESWXWIDGETSWXPYTHON)
	-chown -R root:admin $(EXAMPLESWXWIDGETSWXPYTHON)
	-chmod -R g+w $(EXAMPLESWXWIDGETSWXPYTHON)
	install -d -g admin -m 0775 $(EXAMPLESPYTHON)
	ln -s ../$(PROJECT)/$(WXPYTHONPROJECT) $(EXAMPLESPYTHONWXPYTHON)
	install -d -g admin -m 0775 $(DOCWXWIDGETSWXPYTHON)
	rsync -rlt $(WXPYTHONBUILD)/docs/ $(DOCWXWIDGETSWXPYTHON)
	-chown -R root:admin $(DOCWXWIDGETSWXPYTHON)
	-chmod -R g+w $(DOCWXWIDGETSWXPYTHON)
	install -d -g admin -m 0775 $(DOCPYTHON)
	ln -s ../$(PROJECT)/$(WXPYTHONPROJECT) $(DOCPYTHONWXPYTHON)
	@set -x && \
	for i in `find $(DSTROOT) -name __init__.py -size 0c`; do \
	    echo '#' > $$i && \
	    python -m py_compile $$i; \
	done
	chmod -x $(DSTROOT)/Developer/Examples/wxWidgets/wxPython/samples/wxPIA_book/Chapter-17/sample-text.txt

.DEFAULT:
	@$(MAKE) -f Makefile $@
