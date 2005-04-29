##
# Makefile for tcl
##

Project = tcl

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

## Build settings ##

TCL_DSTROOT           = $(if $(DSTROOT),$(DSTROOT),/tmp/tcl/Release)
TCL_FRAMEWORK_DIR     = $(TCL_DSTROOT)/$(NSDEFAULTLOCATION)/Library/Frameworks
TCLSH                 = $(TCL_DSTROOT)/usr/bin/tclsh
WISH                  = $(TCL_DSTROOT)/usr/bin/wish

NSDEFAULTLOCATION     = /System

MAKE_ARGS             = VERBOSE=YES NSDEFAULTLOCATION=$(NSDEFAULTLOCATION) \
                        TclFramework=$(TCL_FRAMEWORK_DIR)/Tcl.framework Tclsh=$(TCLSH) \
                        TkFramework=$(TCL_FRAMEWORK_DIR)/Tk.framework Wish=$(WISH) \
                        $(if $(UseCvs),UseCvs=$(UseCvs))

core = tcl tk
ext  = tcl_ext
all  = $(core) $(ext)

## targets ##

.PHONY: $(all)

## installsrc ##

install_source::
	@echo "Extracting $(Project)..."
	for subdir in $(all) ; do \
		$(MAKE) -C $(SRCROOT)/$${subdir} -f Makefile.fetch fetch SRCROOT=$(SRCROOT)/$${subdir}; \
	done
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/expect/expect/install-sh
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/expect/expect/mkinstalldirs
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/incrtcl/incrTcl/config/install-sh
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/tclAE/TclAE/Build/Resources/macRoman2utf8.tcl

## install ##

install:: $(all) install-plist munge-docs

$(all):
	$(_v) $(MAKE) -C $@ install $(MAKE_ARGS) \
		SRCROOT=$(SRCROOT)/$@ \
		OBJROOT=$(OBJROOT)/$@ \
		SYMROOT=$(SYMROOT)/$@ \
		DSTROOT=$(DSTROOT)

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/tcl/tcl/license.terms $(OSL)/tcl.txt
	$(INSTALL_FILE) $(SRCROOT)/tk/tk/license.terms $(OSL)/tk.txt

munge-docs:
	$(MKDIR) "$(DSTROOT)$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)"
	$(MV) "$(DSTROOT)/Developer/Documentation/DeveloperTools/Tcl" \
		"$(DSTROOT)$(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)"
	$(RMDIR) "$(DSTROOT)/Developer/Documentation"
