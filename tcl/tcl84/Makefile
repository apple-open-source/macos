##
# Makefile for Tcl
##
# Daniel A. Steffen <das@users.sourceforge.net>:
#   01/Jun/04: modified for tcl 8.4.6, no apple patches needed anymore,
#              all apple specific fixes now done by makefile override
#   25/Jul/04: submitted with framework build support (3333098)
##

# Project info
Project               = tcl
ProjectName           = Tcl
ProjectPlistName      = tcl84
UserType              = Developer
ToolType              = Commands

# Fetch info
Release               = 8.4.19
UrlFile               = $(Project)$(Release)
Url                   = $(UrlBase)/$(Project)/$(ProjectName)/$(Release)/$(UrlFile)-src$(UrlExt)
#CvsRoot               = :pserver:anonymous@tcl.cvs.sourceforge.net:/cvsroot/tcl
#CvsTag                = core-8-4-branch
#ImportDate            = 2008-12-08

Configure             = $(Sources)/unix/configure
Extra_Environment     = INSTALL_PATH="$(NSFRAMEWORKDIR)" PREFIX="$(USRDIR)" \
			SYMROOT="$(SYMROOT)" OBJROOT="$(OBJROOT)" \
			MANDIR="$(MANDIR)" INSTALL_MANPAGES=1 \
			EXTRA_CONFIGURE_ARGS="$(Configure_Flags)" \
			EXTRA_MAKE_ARGS="CFLAGS_WARNING='-Wall'" \
			TCL_PACKAGE_PATH="$(Tcl_Package_Path)"
Extra_CC_Flags        = -DTCL_TIP268 -DTCL_TIP280 -DNDEBUG
AfterInstall          = extra-int-headers fix-tclConfig lipo links compress_man_pages fix-perms

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
include ../tcl_ext/Fetch.make

# Tcl needs to be built thread-safe, using --enable-threads. As of 8.4.4
# the Mac OS X framework Makefile passes --enable-threads to configure
# by default. (3290551)
#Configure_Flags       += --enable-threads

Install_Flags         = INSTALL_ROOT="$(DSTROOT)"

Version               = $(shell $(GREP) "TCL_VERSION=" "$(Configure)" | $(CUT) -d '=' -f 2)

FmwkDir               = $(NSFRAMEWORKDIR)/$(ProjectName).framework/Versions/$(Version)
LibItems              = tclConfig.sh libtclstub$(Version).a
HeaderItems           = tcl.h tclDecls.h tclPlatDecls.h
PrivateHeaderItems    = tclInt.h tclIntDecls.h tclIntPlatDecls.h tclMath.h \
                        tclPort.h tclUnixPort.h tclCompile.h

Tcl_Package_Path      = ~/Library/Tcl /Library/Tcl /System/Library/Tcl \
                        ~/Library/Frameworks /Library/Frameworks /System/Library/Frameworks

# add suffix to tcl manpage names to avoid conflicts (3212727)
Manpage_Suffix        = tcl
Configure_Flags       += --enable-man-suffix=$(Manpage_Suffix)

extra-int-headers:
	$(_v) $(INSTALL_FILE) "$(Sources)/generic/tclCompile.h" \
	        "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(MKDIR) "$(DSTROOT)$(FmwkDir)/Headers/tcl-private"
	$(_v) $(MV) -f $(foreach f,$(PrivateHeaderItems),"$(DSTROOT)$(FmwkDir)/PrivateHeaders/$(f)") "$(DSTROOT)$(FmwkDir)/Headers/tcl-private"
	$(_v) $(RMDIR) "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(LN) -fsh "Headers/tcl-private" "$(DSTROOT)$(FmwkDir)/PrivateHeaders"

fix-tclConfig:
	$(_v) $(SED) -e "s#$(CFLAGS)##g" -e "s#$(LDFLAGS)##g" "$(DSTROOT)$(FmwkDir)/tclConfig.sh" > "$(SYMROOT)/tclConfig.sh"
	$(_v) $(INSTALL_FILE) "$(SYMROOT)/tclConfig.sh" "$(DSTROOT)$(FmwkDir)"

lipo_archs := $(filter ppc ppc64 x86_64,$(RC_ARCHS))
ifdef lipo_archs
define lipo
	$(_v) lipo $(patsubst %,-remove %,$(patsubst ppc,ppc7400,$(lipo_archs))) -output $(1) $(1)
endef
endif
lipo:
	$(call lipo,$(DSTROOT)$(USRBINDIR)/tclsh$(Version))

links:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(LN) -fs "../..$(FmwkDir)/$(ProjectName)" "$(DSTROOT)$(USRLIBDIR)/libtcl$(Version).dylib"
	$(_v) $(LN) -fs "libtcl$(Version).dylib" "$(DSTROOT)$(USRLIBDIR)/libtcl.dylib"
	$(_v) $(LN) -fs $(foreach f,$(LibItems),"../..$(FmwkDir)/$(f)") "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRINCLUDEDIR)"
	$(_v) $(LN) -fs $(foreach f,$(HeaderItems),"../..$(FmwkDir)/Headers/$(f)") \
	        "$(DSTROOT)$(USRINCLUDEDIR)"
# Install compat sym links for private headers in /usr/local/include (3198305)
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRDIR)/local/include"
	$(_v) $(LN) -fs $(foreach f,$(PrivateHeaderItems),"../../..$(FmwkDir)/Headers/tcl-private/$(f)") \
	        "$(DSTROOT)$(USRDIR)/local/include/"
# Move tclsh.1 to tclsh$(Version).1 and link tclsh.1 to tclsh$(Version).1 (2853545)
	$(_v) $(MV) -f "$(DSTROOT)$(MANDIR)/man1/tclsh.1$(Manpage_Suffix)" \
	        "$(DSTROOT)$(MANDIR)/man1/tclsh$(Version).1$(Manpage_Suffix)"
	$(_v) $(LN) -fs "tclsh$(Version).1$(Manpage_Suffix)" \
	        "$(DSTROOT)$(MANDIR)/man1/tclsh.1$(Manpage_Suffix)"
# Move tclsh binary into framework and install symlink to it in $(USRBINDIR)
	$(_v) $(MV) -f "$(DSTROOT)$(USRBINDIR)/tclsh$(Version)" "$(DSTROOT)$(FmwkDir)"
	$(_v) $(LN) -fs "../..$(FmwkDir)/tclsh$(Version)" "$(DSTROOT)$(USRBINDIR)/"

fix-perms:
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT)
	$(_v)- $(FIND) $(DSTROOT) -type f ! -perm -+x ! -perm $(Install_File_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_File_Mode)
	$(_v)- $(FIND) $(DSTROOT) -type f -perm -+x ! -perm $(Install_Program_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_Program_Mode)
	$(_v)- $(FIND) $(DSTROOT) -type d ! -perm $(Install_Directory_Mode) -print0 | $(XARGS) -0 $(CHMOD) $(Install_Directory_Mode)

###

Sources               = $(SRCROOT)/$(Project)
MakeDir               = $(Sources)/macosx

Fmwk                  = $(SYMROOT)/$(ProjectName).framework
BuildTarget           = $(Project) BUILD_STYLE=Deployment
InstallTarget         = install-deploy

all: build

build:: $(Fmwk) $(Fmwk).dSYM

$(Fmwk):
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(MakeDir) $(BuildTarget)

$(Fmwk).dSYM: $(Fmwk)
	$(_v) $(DSYMUTIL) -o $(Fmwk).dSYM $(Fmwk)/$(ProjectName)
	$(call lipo,$(SYMROOT)/tclsh$(Version))
	$(_v) $(DSYMUTIL) $(SYMROOT)/tclsh$(Version)

install:: $(Fmwk) $(Fmwk).dSYM
	@echo "Installing $(Project)..."
	-@$(RM) -f "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) umask $(Install_Mask) ; $(Environment) $(MAKE) -C $(MakeDir) \
	        $(Install_Flags) $(InstallTarget)
	$(_v) $(RM) -f $(DSTROOT)$(MANDIR)/man{n/resource.n,3/Macintosh.3}$(Manpage_Suffix)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) -print0 | $(XARGS) -0 $(RMDIR)
	$(_v) $(MAKE) $(AfterInstall)

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(MakeDir) clean-$(BuildTarget)

extract::
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/mac

.PHONY: almostclean $(AfterInstall)
.NOTPARALLEL:
