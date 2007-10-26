##
# Makefile support for building Tcl extensions
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

##
# CoreOS Makefile settings common to all Tcl extensions
##

UserType              = Developer
ToolType              = Libraries

ifndef CoreOSMakefiles
ifneq ($(TEA_UseXcode),YES)
# Tcl extensions are GNU Source projects
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
else
# Xcode based Tcl extensions
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
endif
endif

Install_Flags         = DESTDIR="$(DSTROOT)" $(Extra_Install_Flags)
Install_Target        = install

##
# Definitions used by all Tcl extensions
##

TclExtLibDir          = $(NSLIBRARYDIR)/Tcl
TclExtManDir          = $(MANDIR)/mann
TclExtHtmlDir         = $(NSDEVELOPERDIR)/Documentation/DeveloperTools/Tcl/$(ProjectName)

Tclsh                 = $(USRBINDIR)/tclsh
TclFramework          = $(NSFRAMEWORKDIR)/Tcl.framework
TclHeaders            = $(TclFramework)/Headers
TclPrivateHeaders     = $(TclFramework)/PrivateHeaders

Wish                  = $(USRBINDIR)/wish
TkFramework           = $(NSFRAMEWORKDIR)/Tk.framework
TkHeaders             = $(TkFramework)/Headers
TkPrivateHeaders      = $(TkFramework)/PrivateHeaders

TclExtDir             = $(Project)
TclExtStubConfig      = $(TclExtDir)Config.sh

TEA_Configure_Flags   = --libdir=$(TclExtLibDir) \
                        --with-tcl=$(TclFramework) \
                        --with-tclinclude=$(TclHeaders) \
                        --with-tk=$(TkFramework) \
                        --with-tkinclude=$(TkHeaders) \
                        --enable-threads \
                        $(Extra_TEA_Configure_Flags)
TEA_Environment       = TCLSH_PROG=$(Tclsh) \
                        $(Extra_TEA_Environment)

export PATH           := $(shell dirname $(Tclsh)):$(shell dirname $(Wish)):$(PATH)
export DYLD_FRAMEWORK_PATH := $(shell dirname $(TclFramework)):$(shell dirname $(TkFramework))

##
# Common cleanup actions
##

# Remove empty directories from DSTROOT after install
Find_Cruft            := '(' $(Find_Cruft) ')' -or '(' -mindepth 1 -type d -empty -print ')'

# strip debugging symbols from installed extensions
strip:
	$(_v) shopt -s nullglob && $(STRIP) -S "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtDir)*/*.{dylib,a}

# move project stub config file to $(USRLIBDIR) where it belongs
fix-config:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(USRLIBDIR)"
	$(_v) $(MV) -f "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtStubConfig) \
	    "$(DSTROOT)/$(USRLIBDIR)"

# fix owner after custom after-install actions
fix-owner:
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT)

#avoid complaints from Find_Cruft and GnuChown about non existing SYMROOT
build::
	$(_v)- $(MKDIR) $(SYMROOT)

##
# Definitions used by Xcode based extensions
##
ifeq ($(TEA_UseXcode),YES)

Sources               = $(SRCROOT)/$(Project)
XCODEBUILD            = /usr/bin/xcodebuild
XcodeBuild            = cd $(Sources) && $(XCODEBUILD) -configuration Deployment \
                        $(MAKEOVERRIDES) CC=gcc \
                        SRCROOT="$(Sources)" OBJROOT="$(OBJROOT)" SYMROOT="$(SYMROOT)" \
                        $(Environment) $(Extra_Xcode_Flags)

build::
	@echo "Building $(Project)..."
	$(_v) $(MKDIR) $(OBJROOT)/Deployment.build/$(ProjectName).build
	$(_v) $(LN) -fs -fs Deployment.build/$(ProjectName).build $(OBJROOT)/$(ProjectName).build
	$(_v) $(XcodeBuild)

install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; \
	    $(XcodeBuild) INSTALL_ROOT="$(DSTROOT)"  $(Install_Flags) $(Install_Target)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
ifdef AfterInstall
	$(_v) $(MAKE) $(AfterInstall)
endif

endif


.PHONY: strip install-doc fix-config fix-owner
