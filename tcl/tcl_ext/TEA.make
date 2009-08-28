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

ifneq ($(Configure),:)
Configure_Products   += config.log
else
Configure_Products    = .
endif

# avoid complaints about non-existing SYMROOT
configure::
	$(_v)- $(MKDIR) $(SYMROOT)

ifndef CoreOSMakefiles
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
endif

Install_Flags         = DESTDIR="$(DSTROOT)" $(Extra_Install_Flags)
Install_Target        = install

##
# Definitions used by all Tcl extensions
##

TclExtLibDir          = $(NSLIBRARYDIR)/Tcl
TclExtManDir          = $(MANDIR)/mann
TclExtHtmlDir         = $(SYSTEM_DEVELOPER_TOOLS_DOC_DIR)/Tcl/$(ProjectName)

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
                        --with-tk=$(TkFramework) \
                        --enable-threads \
                        $(Extra_TEA_Configure_Flags)
TEA_Environment       = TCLSH_PROG=$(Tclsh) \
                        $(Extra_TEA_Environment)
TEA_TclConfig         = $(SRCROOT)/../tclconfig

export PATH           := $(shell dirname $(Tclsh)):$(shell dirname $(Wish)):$(PATH)
export DYLD_FRAMEWORK_PATH := $(shell dirname $(TclFramework)):$(shell dirname $(TkFramework))

##
# Common cleanup actions
##

# Remove empty directories from DSTROOT after install
Find_Cruft            := '(' '(' $(Find_Cruft) ')' -or '(' -mindepth 1 -type d -empty -print ')' ')'

# strip installed extension libraries, fix install_name, delete stub library
strip::
	$(_v) shopt -s nullglob && cd "$(DSTROOT)" && \
	$(RM) -f "./$(TclExtLibDir)"/$(TclExtDir)*/lib*stub*.a && \
	$(STRIP) -S "./$(TclExtLibDir)"/$(TclExtDir)*/*.{dylib,a} && \
	for f in "./$(TclExtLibDir)"/$(TclExtDir)*/*.dylib; do \
	install_name_tool -id "$${f:2}" "$${f}"; done

# delete stub config file
fix-config:
	$(_v) $(RM) -f "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtStubConfig)

# fix permissions and owner after install
fix-perms::
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT)
	$(_v)- $(FIND) "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtDir)* -type f ! -name "*.dylib" -perm -+x -print0 | $(XARGS) -0 $(CHMOD) -x
	$(_v)- $(FIND) "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtDir)* -type f -name "*.dylib" ! -perm -+x -print0 | $(XARGS) -0 $(CHMOD) +x
	$(_v)- $(FIND) $(DSTROOT) -type f ! -perm -+x ! -perm $(Install_File_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_File_Mode)
	$(_v)- $(FIND) $(DSTROOT) -type f -perm -+x ! -perm $(Install_Program_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_Program_Mode)
	$(_v)- $(FIND) $(DSTROOT) -type d ! -perm $(Install_Directory_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_Directory_Mode)

.PHONY: strip install-doc fix-config fix-perms
.NOTPARALLEL:

include ../Fetch.make
