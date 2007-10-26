##
# Makefile for tcl
##

Project = tcl

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

## Build settings ##

TCL_DSTROOT           = $(if $(DSTROOT),$(DSTROOT),/tmp/tcl/Release)
TCL_FRAMEWORK_DIR     = $(TCL_DSTROOT)/$(NSDEFAULTLOCATION)/Library/Frameworks
TCLSH                 = /usr/bin/tclsh
WISH                  = /usr/bin/wish

AC_VALS               = ac_cv_path_tclsh=$(TCLSH) ac_cv_path_wish=$(WISH) ac_cv_header_stdc=yes
NSDEFAULTLOCATION     = /System

MAKE_ARGS             = VERBOSE=YES NSDEFAULTLOCATION=$(NSDEFAULTLOCATION) \
                        TclFramework=$(TCL_FRAMEWORK_DIR)/Tcl.framework Tclsh=$(TCLSH) \
                        TkFramework=$(TCL_FRAMEWORK_DIR)/Tk.framework Wish=$(WISH) \
                        $(if $(UseCvs),UseCvs=$(UseCvs)) $(AC_VALS)

MAKE_ARGS_only          = VERBOSE=YES NSDEFAULTLOCATION=$(NSDEFAULTLOCATION) \
                        TclFramework=/System/Library/Frameworks/Tcl.framework Tclsh=$(TCLSH) \
                        TkFramework=/System/Library/Frameworks/Tk.framework Wish=$(WISH) \
                        $(if $(UseCvs),UseCvs=$(UseCvs)) $(AC_VALS)

core = tcl tk
ext  = tcl_ext
all  = $(core) $(ext)

## targets ##

.PHONY: $(all)

## installsrc ##

install_source::
	@echo "Extracting $(Project)..."
	for subdir in $(all) ; do \
		$(MAKE) -C $(SRCROOT)/$${subdir} -f Makefile.fetch fetch SRCROOT=$(SRCROOT)/$${subdir} || exit 1; \
	done
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/expect/expect/install-sh
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/expect/expect/mkinstalldirs
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/incrtcl/incrTcl/config/install-sh
	$(CHMOD) a+x $(SRCROOT)/tcl_ext/tclAE/TclAE/Build/Resources/macRoman2utf8.tcl
	# repair broken scripts with unbalanced single quotes
	find $(SRCROOT) -type f -print0 | xargs -0 perl -pi -e 's:( /etc/.relid)\x27:\1:' 

## install ##

install:: $(all) fix-install_names install-plist munge-docs compress_man_pages

install_tcl:: tcl install-plist compress_man_pages

install_tk:: tk_only compress_man_pages

install_tcl_ext:: tcl_ext_only fix-install_names munge-docs compress_man_pages

$(all):
	$(_v) unset LD_SPLITSEGS_NEW_LIBRARIES && \
		$(_v) $(MAKE) -C $@ install $(MAKE_ARGS) \
		SRCROOT=$(SRCROOT)/$@ \
		OBJROOT=$(OBJROOT)/$@ \
		SYMROOT=$(SYMROOT)/$@ \
		DSTROOT=$(DSTROOT)
	rm -f $(DSTROOT)/System/Library/Tcl/*/lib*stub*.a

tk_only:
	$(_v) unset LD_SPLITSEGS_NEW_LIBRARIES && \
		$(_v) $(MAKE) -C tk install $(MAKE_ARGS_only) \
		SRCROOT=$(SRCROOT)/tk \
		OBJROOT=$(OBJROOT)/tk \
		SYMROOT=$(SYMROOT)/tk \
		DSTROOT=$(DSTROOT)
tcl_ext_only:
	$(_v) unset LD_SPLITSEGS_NEW_LIBRARIES && \
		$(_v) $(MAKE) -C tcl_ext install $(MAKE_ARGS_only) \
		SRCROOT=$(SRCROOT)/tcl_ext \
		OBJROOT=$(OBJROOT)/tcl_ext \
		SYMROOT=$(SYMROOT)/tcl_ext \
		DSTROOT=$(DSTROOT)
	rm -f $(DSTROOT)/System/Library/Tcl/*/lib*stub*.a

fix-install_names:
	cd $(DSTROOT); perl -e 'foreach (glob "System/Library/Tcl/*/*.dylib") {print "running install_name_tool -id /$$_ $$_\n";system "install_name_tool -id /$$_ $$_";}'

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
	rmdir $(DSTROOT)/Developer/Documentation/DeveloperTools
