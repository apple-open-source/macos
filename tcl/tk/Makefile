##
# Makefile for Tk
##
# Daniel A. Steffen <das@users.sourceforge.net>:
##

# Project info
Project               = tk
ProjectName           = Tk
UserType              = Developer
ToolType              = Commands
Configure             = $(Sources)/unix/configure
Extra_Environment     = DYLIB_INSTALL_PATH="$(NSFRAMEWORKDIR)" PREFIX="$(USRDIR)" \
                        APPLICATION_INSTALL_PATH="$(AppInstallDir)" CC=gcc \
			SYMROOT="$(SYMROOT)" OBJROOT="$(OBJROOT)" \
			MANDIR="$(MANDIR)" INSTALL_MANPAGES=1 \
			DOCDIR="$(DocDir)" MKLINKS_FLAGS="$(MkLinks_Flags)" \
			TCL_FRAMEWORK_DIR="$(TclFrameworkDir)" TCLSH_DIR="$(TclshDir)" \
			TCLTKMAN2HTML="$(TclTkMan2Html)" LDFLAGS=""
AfterInstall          = extra-int-headers links

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

CC_Archs              = 

Install_Flags         = INSTALL_ROOT="$(DSTROOT)" 

Version               = $(shell $(GREP) "TK_VERSION=" "$(Configure)" | $(CUT) -d '=' -f 2)

FmwkDir               = $(NSFRAMEWORKDIR)/Tk.framework/Versions/$(Version)
LibItems              = tkConfig.sh libtkstub$(Version).a
HeaderItems           = tk.h tkDecls.h tkPlatDecls.h tkIntXlibDecls.h tkMacOSX.h
PrivateHeaderItems    = tkInt.h tkIntDecls.h tkIntPlatDecls.h tkMacOSXInt.h \
                        tkPort.h tkMacOSXPort.h default.h tkMacOSXDefault.h tkFont.h
Wish                  = Wish Shell.app
AppInstallDir         = $(NSDEVELOPERAPPSDIR)/Utilities

Tclsh                 = $(USRBINDIR)/tclsh
TclFramework          = $(NSFRAMEWORKDIR)/Tcl.framework

TclshDir              = $(shell dirname $(Tclsh))
TclFrameworkDir       = $(shell dirname $(TclFramework))

TclTkMan2Html         = $(CURDIR)/tcltk-man2html.tcl

# HTML Documentation should not be installed under English.lproj
DocDir                = $(FmwkDir)/Resources/Documentation/Reference

# add suffix to tk manpage names to avoid conflicts, force case insensitivity check
Manpage_Suffix        = tcl
MkLinks_Flags         = -z 'eval CASEINSENSITIVEFS=1 && Z=$(Manpage_Suffix) && S=-f && \
			$(SH) -c \"$(MV) \\\$$\$$1 \\\$$\$$1\$$\$$Z\" \"\"'

extra-int-headers:
	$(_v) $(INSTALL_FILE) "$(Sources)/generic/default.h" "$(Sources)/macosx/tkMacOSXDefault.h" \
	        "$(Sources)/generic/tkFont.h" "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(MKDIR)  "$(DSTROOT)$(FmwkDir)/Headers/tk-private"
	$(_v) $(MV) $(foreach f,$(PrivateHeaderItems),"$(DSTROOT)$(FmwkDir)/PrivateHeaders/$(f)") "$(DSTROOT)$(FmwkDir)/Headers/tk-private"
	$(_v) $(RMDIR)  "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(LN) -fs "Headers/tk-private" "$(DSTROOT)$(FmwkDir)/PrivateHeaders"

links:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(LN) -fs "../..$(FmwkDir)/$(ProjectName)" "$(DSTROOT)/usr/lib/libtk$(Version).dylib"
	$(_v) $(LN) -fs "libtk$(Version).dylib" "$(DSTROOT)$(USRLIBDIR)/libtk.dylib"
	$(_v) $(LN) -fs $(foreach f,$(LibItems),"../..$(FmwkDir)/$(f)") "$(DSTROOT)/usr/lib"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRINCLUDEDIR)"
	$(_v) $(LN) -fs $(foreach f,$(HeaderItems),"../..$(FmwkDir)/Headers/$(f)") \
	        "$(DSTROOT)/usr/include"
# Install compat sym links for private headers in /usr/local/include
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/usr/local/include"
	$(_v) $(LN) -fs $(foreach f,$(PrivateHeaderItems),"../../..$(FmwkDir)/Headers/tk-private/$(f)") \
	        "$(DSTROOT)/usr/local/include/"
# Move wish.1 to wish$(Version).1 and link wish.1 to wish$(Version).1
	$(_v) $(MV) "$(DSTROOT)$(MANDIR)/man1/wish.1$(Manpage_Suffix)" \
	        "$(DSTROOT)$(MANDIR)/man1/wish$(Version).1$(Manpage_Suffix)"
	$(_v) $(LN) -fs "wish$(Version).1$(Manpage_Suffix)" \
	        "$(DSTROOT)$(MANDIR)/man1/wish.1$(Manpage_Suffix)"
# Remove /Developer/Applications (3908113)
	$(_v) $(RMDIR) $(DSTROOT)$(NSDEVELOPERAPPSDIR)

###

Sources               = $(SRCROOT)/$(Project)
MakeDir               = $(Sources)/macosx

Fmwk                  = $(SYMROOT)/Tk.framework
BuildTarget           = deploy

all: build

.PHONY: almostclean $(AfterInstall)

build:: $(Fmwk)

$(Fmwk):
	@echo "Building $(Project)..."
	$(_v) $(MKDIR) $(OBJROOT)/Deployment.build/Wish.build
	$(_v) $(LN) -fs Deployment.build/Wish.build $(OBJROOT)/Wish.build
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) $(BuildTarget)

install:: $(Fmwk)
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(MakeDir) $(Environment) \
	        $(Install_Flags) install-$(BuildTarget)
# xcode makes dirs read-only, need write access for AfterInstall
	$(_v) $(CHMOD) -RH u+w "$(DSTROOT)/$(FmwkDir)" "$(DSTROOT)/$(AppInstallDir)"
	$(_v) $(MAKE) $(AfterInstall)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) -print0 | $(XARGS) -0 $(RMDIR)
# fix owners & permissions
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT)
	$(_v) $(FIND) $(DSTROOT) -type f ! -perm -u+x -print0 | \
	        $(XARGS) -0 $(CHMOD) $(Install_File_Mode)
	$(_v) $(FIND) $(DSTROOT) -type f -perm -u+x -print0 | \
	        $(XARGS) -0 $(CHMOD) $(Install_Program_Mode)
	$(_v) $(FIND) $(DSTROOT) -type d -print0 | \
	        $(XARGS) -0 $(CHMOD) $(Install_Directory_Mode)
	SYMROOT="$(SYMROOT)" DSTROOT="$(DSTROOT)" $(SRCROOT)/fix_tkConfig.sh

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) clean-$(BuildTarget)
