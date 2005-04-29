##---------------------------------------------------------------------
# GNUmakefile for wxWidgets
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory.  The wxPythonSrc
# tarball contains both wxWidgets and wxPython
##---------------------------------------------------------------------
PROJECT = wxWidgets
NAME = wxPython-src
VERSION = 2.5.3.1
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.gz
FAKEBIN = $(OBJROOT)/bin
WXWIDGETSTOP = $(OBJROOT)/$(PROJECT)
WXWIDGETSBUILD = $(WXWIDGETSTOP)/darwin

WXPERLPROJECT = wxPerl
WXPERLNAME = Wx
WXPERLVERSION = 0.22
WXPERLNAMEVERS = $(WXPERLNAME)-$(WXPERLVERSION)
WXPERLTARBALL = $(WXPERLNAMEVERS).tar.gz
WXPERLBUILD = $(OBJROOT)/$(WXPERLPROJECT)/$(WXPERLPROJECT)

WXPYTHONPROJECT = wxPython
WXPYTHONPROJECTVERS = $(WXPYTHONPROJECT)-$(VERSION)
WXPYTHONBUILD = $(OBJROOT)/$(PROJECT)/$(WXPYTHONPROJECT)

DOC = $(DSTROOT)/Developer/Documentation
DOCPERL = $(DOC)/Perl
DOCPERLWXPERL = $(DOCPERL)/$(WXPERLPROJECT)
DOCPYTHON = $(DOC)/Python
DOCPYTHONWXPYTHON = $(DOCPYTHON)/$(WXPYTHONPROJECT)
DOCWXWIDGETS = $(DOC)/$(PROJECT)
DOCWXWIDGETSWXPERL = $(DOCWXWIDGETS)/$(WXPERLPROJECT)
DOCWXWIDGETSWXPYTHON = $(DOCWXWIDGETS)/$(WXPYTHONPROJECT)
EXAMPLES = $(DSTROOT)/Developer/Examples
EXAMPLESPERL = $(EXAMPLES)/Perl
EXAMPLESPERLWXPERL = $(EXAMPLESPERL)/$(WXPERLPROJECT)
EXAMPLESPYTHON = $(EXAMPLES)/Python
EXAMPLESPYTHONWXPYTHON = $(EXAMPLESPYTHON)/$(WXPYTHONPROJECT)
EXAMPLESWXWIDGETS = $(EXAMPLES)/$(PROJECT)
EXAMPLESWXWIDGETSWXPERL = $(EXAMPLESWXWIDGETS)/$(WXPERLPROJECT)
EXAMPLESWXWIDGETSWXPYTHON = $(EXAMPLESWXWIDGETS)/$(WXPYTHONPROJECT)

ifndef LD_TWOLEVEL_NAMESPACE
export LD_TWOLEVEL_NAMESPACE = YES
endif
ifndef MACOSX_DEPLOYMENT_TARGET
export MACOSX_DEPLOYMENT_TARGET = 10.4
endif

no_target: $(PROJECT) $(WXPERLPROJECT) $(WXPYTHONPROJECT)

$(PROJECT): $(WXWIDGETSBUILD)
	$(MAKE) -C $(OBJROOT) -f Makefile \
	    SRCROOT=$(OBJROOT) OBJROOT=$(WXWIDGETSBUILD)

$(WXPERLPROJECT): $(WXPERLBUILD) $(FAKEBIN)/wx-config
	$(MAKE) -C $(OBJROOT)/$(WXPERLPROJECT) FAKEBIN=$(FAKEBIN)

$(WXPYTHONPROJECT): $(WXWIDGETSBUILD) $(FAKEBIN)/wx-config
	$(MAKE) -C $(OBJROOT) -f Makefile.wxPython \
	    OBJROOT=$(WXPYTHONBUILD) FAKEBIN=$(FAKEBIN)

$(FAKEBIN)/wx-config:
	mkdir -p $(FAKEBIN)
	cp $(DSTROOT)/usr/bin/wx-config $(FAKEBIN)/wx-config
	sed 's,XXXDSTROOTXXX,$(DSTROOT),' $(SRCROOT)/fix/wx-config.ed | ed - $(FAKEBIN)/wx-config 

$(WXWIDGETSBUILD):
	rsync -a $(SRCROOT)/ $(OBJROOT)
	@echo cd $(OBJROOT) && \
	cd $(OBJROOT) && \
	echo gnutar xzf $(TARBALL) && \
	gnutar xzf $(TARBALL) && \
	echo rm -rf $(PROJECT) && \
	rm -rf $(PROJECT) && \
	echo mv $(NAMEVERS) $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	echo ed - $(PROJECT)/src/common/dynlib.cpp \< fix/dynlib.cpp.ed && \
	ed - $(PROJECT)/src/common/dynlib.cpp < fix/dynlib.cpp.ed && \
	echo ed - $(PROJECT)/src/mac/carbon/dnd.cpp \< fix/dnd.cpp.ed && \
	ed - $(PROJECT)/src/mac/carbon/dnd.cpp < fix/dnd.cpp.ed && \
	echo ed - $(PROJECT)/Makefile.in \< fix/Makefile.in.ed && \
	ed - $(PROJECT)/Makefile.in < fix/Makefile.in.ed && \
	echo ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.c \< fix/MoreFilesX.c.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.c < fix/MoreFilesX.c.ed && \
	echo ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.h \< fix/MoreFilesX.h.ed && \
	ed - $(PROJECT)/src/mac/carbon/morefilex/MoreFilesX.h < fix/MoreFilesX.h.ed && \
	echo ed - $(PROJECT)/src/mac/carbon/fontdlg.cpp \< fix/fontdlg.cpp.ed && \
	ed - $(PROJECT)/src/mac/carbon/fontdlg.cpp < fix/fontdlg.cpp.ed && \
	for i in configure src/html/htmlctrl/webkit/webkit.mm; do \
	    echo ed - $(PROJECT)/$$i \< fix/WebKit.ed && \
	    ed - $(PROJECT)/$$i < fix/WebKit.ed; \
	done
	mkdir $(WXWIDGETSBUILD)

$(WXPERLBUILD):
	@echo cd $(OBJROOT)/$(WXPERLPROJECT) && \
	cd $(OBJROOT)/$(WXPERLPROJECT) && \
	echo gnutar xzf $(WXPERLTARBALL) && \
	gnutar xzf $(WXPERLTARBALL) && \
	echo rm -rf $(WXPERLPROJECT) && \
	rm -rf $(WXPERLPROJECT) && \
	echo mv $(WXPERLNAMEVERS) $(WXPERLPROJECT) && \
	mv $(WXPERLNAMEVERS) $(WXPERLPROJECT) && \
	echo ed - $(WXPERLPROJECT)/ext/grid/XS/Grid.xs \< Grid.xs.ed && \
	ed - $(WXPERLPROJECT)/ext/grid/XS/Grid.xs < Grid.xs.ed && \
	echo ed - $(WXPERLPROJECT)/ext/stc/cpp/st_constants.cpp \< st_constants.cpp.ed && \
	ed - $(WXPERLPROJECT)/ext/stc/cpp/st_constants.cpp < st_constants.cpp.ed

install: $(PROJECT)install $(WXPERLPROJECT)install $(WXPYTHONPROJECT)install

$(PROJECT)install: $(WXWIDGETSBUILD)
	$(MAKE) -C $(OBJROOT) -f Makefile install \
	    SRCROOT=$(OBJROOT) OBJROOT=$(WXWIDGETSBUILD)
	rm -rf $(DSTROOT)/usr/share/locale
	install -d -g admin -m 0775 $(DOCWXWIDGETS)
	rsync -rlt $(WXWIDGETSTOP)/docs $(DOCWXWIDGETS)
	-chown -R root:admin $(DOCWXWIDGETS)
	-chmod -R g+w $(DOCWXWIDGETS)

$(WXPERLPROJECT)install: $(WXPERLBUILD) $(FAKEBIN)/wx-config
	$(MAKE) -C $(OBJROOT)/$(WXPERLPROJECT) install FAKEBIN=$(FAKEBIN)
	install -d -g admin -m 0775 $(EXAMPLESWXWIDGETSWXPERL)
	rsync -rlt $(WXPERLBUILD)/demo $(WXPERLBUILD)/samples $(EXAMPLESWXWIDGETSWXPERL)
	-chown -R root:admin $(EXAMPLESWXWIDGETSWXPERL)
	-chmod -R g+w $(EXAMPLESWXWIDGETSWXPERL)
	install -d -g admin -m 0775 $(EXAMPLESPERL)
	ln -s ../$(PROJECT)/$(WXPERLPROJECT) $(EXAMPLESPERLWXPERL)
	install -d -g admin -m 0775 $(DOCWXWIDGETSWXPERL)
	rsync -rlt $(WXPERLBUILD)/README.txt $(WXPERLBUILD)/docs/ $(DOCWXWIDGETSWXPERL)
	-chown -R root:admin $(DOCWXWIDGETSWXPERL)
	-chmod -R g+w $(DOCWXWIDGETSWXPERL)
	install -d -g admin -m 0775 $(DOCPERL)
	ln -s ../$(PROJECT)/$(WXPERLPROJECT) $(DOCPERLWXPERL)

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

.DEFAULT:
	@$(MAKE) -f Makefile $@
