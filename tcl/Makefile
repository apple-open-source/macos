##
# Makefile for Tcl
##

# Project info
Project               = tcl
UserType              = Developer
ToolType              = Commands
Configure             = $(Sources)/unix/configure
Extra_Environment     = INSTALL_PATH="$(NSFRAMEWORKDIR)" PREFIX="$(USRDIR)" \
			BUILD_DIR="$(BuildDirectory)" TCL_EXE="$(Tclsh)" \
			MANDIR="$(MANDIR)" INSTALL_MANPAGES=1
AfterInstall          = extra-int-headers links old-tcllib

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
# Tcl needs to be built thread-safe, using --enable-threads. As of 8.4.4
# the Mac OS X framework Makefile passes --enable-threads to configure
# by default. (3290551)
#Configure_Flags       += --enable-threads

Install_Flags = INSTALL_ROOT="$(DSTROOT)"

Version = $(shell $(GREP) "TCL_VERSION=" "$(Configure)" | $(CUT) -d '=' -f 2)

FmwkDir               = $(NSFRAMEWORKDIR)/Tcl.framework/Versions/$(Version)
LibItems              = tclConfig.sh libtclstub$(Version).a
HeaderItems           = tcl.h tclDecls.h tclPlatDecls.h
PrivateHeaderItems    = tclInt.h tclIntDecls.h tclIntPlatDecls.h tclMath.h tclPort.h

extra-int-headers:
	$(_v) $(INSTALL_FILE) "$(Sources)/unix/tclUnixPort.h" "$(DSTROOT)$(FmwkDir)/PrivateHeaders/tclPort.h"

links:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(LN) -fs "$(FmwkDir)/Tcl" "$(DSTROOT)$(USRLIBDIR)/libtcl$(Version).dylib"
	$(_v) $(LN) -fs "libtcl$(Version).dylib" "$(DSTROOT)$(USRLIBDIR)/libtcl.dylib"
	$(_v) $(LN) -fs $(foreach f,$(LibItems),"$(FmwkDir)/$(f)") "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRINCLUDEDIR)"
	$(_v) $(LN) -fs $(foreach f,$(HeaderItems),"$(FmwkDir)/Headers/$(f)") \
	        "$(DSTROOT)$(USRINCLUDEDIR)"
# Install compat sym links for private headers in /usr/local/include (3198305)
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/usr/local/include"
	$(_v) $(LN) -fs $(foreach f,$(PrivateHeaderItems),"$(FmwkDir)/PrivateHeaders/$(f)") \
	        "$(DSTROOT)/usr/local/include/"
# Move tclsh.1 to tclsh$(Version).1 and
# link tclsh.1 to tclsh$(Version).1 (2853545)
	$(_v) $(MV) "$(DSTROOT)$(MANDIR)/man1/tclsh.1" "$(DSTROOT)$(MANDIR)/man1/tclsh$(Version).1"
	$(_v) $(LN) -fs "tclsh$(Version).1" "$(DSTROOT)$(MANDIR)/man1/tclsh.1"

# Provide Tcl 8.3 dylib for binary compatibility (3280206)
old-tcllib:
	$(_v) cd $(BuildDirectory) && /usr/bin/uudecode $(SRCROOT)/libtcl8.3.dylib.uue
	$(_v) $(INSTALL_DYLIB) "$(BuildDirectory)/libtcl8.3.dylib" "$(DSTROOT)$(USRLIBDIR)"

###

Sources               = $(SRCROOT)/$(Project)
MakeDir               = $(Sources)/macosx

BuildTarget           = deploy

all: build-$(Project)

.PHONY: almostclean build-$(Project) $(AfterInstall)

build-$(Project):
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) $(BuildTarget)

install::
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(MakeDir) $(Environment) \
	        $(Install_Flags) install-$(BuildTarget)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v)- $(CHOWN) -R $(Install_User).$(Install_Group) $(DSTROOT)
	$(_v) $(MAKE) $(AfterInstall)

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) clean-$(BuildTarget)
