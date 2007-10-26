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
UserType              = Developer
ToolType              = Commands
Configure             = $(Sources)/unix/configure
Extra_Environment     = INSTALL_PATH="$(NSFRAMEWORKDIR)" PREFIX="$(USRDIR)" \
			SYMROOT="$(SYMROOT)" OBJROOT="$(OBJROOT)" \
			MANDIR="$(MANDIR)" INSTALL_MANPAGES=1 \
			DOCDIR="$(DocDir)" MKLINKS_FLAGS="$(MkLinks_Flags)"
Extra_CC_Flags        = -DTCL_IO_TRACK_OS_FOR_DRIVER_WITH_BAD_BLOCKING
AfterInstall          = extra-int-headers links lipo64

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make
# Tcl needs to be built thread-safe, using --enable-threads. As of 8.4.4
# the Mac OS X framework Makefile passes --enable-threads to configure
# by default. (3290551)
#Configure_Flags       += --enable-threads

Install_Flags = INSTALL_ROOT="$(DSTROOT)"

Version = $(shell $(GREP) "TCL_VERSION=" "$(Configure)" | $(CUT) -d '=' -f 2)

FmwkDir               = $(NSFRAMEWORKDIR)/Tcl.framework/Versions/$(Version)
LibItems              = tclConfig.sh libtclstub$(Version).a
HeaderItems           = tcl.h tclDecls.h tclPlatDecls.h tclArch.h
PrivateHeaderItems    = tclInt.h tclIntDecls.h tclIntPlatDecls.h tclMath.h \
                        tclPort.h tclUnixPort.h tclCompile.h 

# HTML Documentation should not be installed under English.lproj (3333098)
DocDir                = \$${LIBDIR}/Resources/Documentation/Reference

# add suffix to tcl manpage names to avoid conflicts, force case insensitivity check (3212727)
Manpage_Suffix        = tcl
MkLinks_Flags         = -z 'eval CASEINSENSITIVEFS=1 && Z=$(Manpage_Suffix) && S=-f && \
			$(SH) -c \"$(MV) \\\$$\$$1 \\\$$\$$1\$$\$$Z\" \"\"'

extra-int-headers:
# correct relative path to tclUnixPort.h for install location 
	$(_v) $(SED) -e 's#\.\./unix/\(tclUnixPort\.h\)#\1#' \
	        "$(Sources)/generic/tclPort.h" > "$(DSTROOT)$(FmwkDir)/PrivateHeaders/tclPort.h"
	$(_v) $(INSTALL_FILE) "$(Sources)/unix/tclUnixPort.h" "$(Sources)/generic/tclCompile.h" \
	        "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(MKDIR)  "$(DSTROOT)$(FmwkDir)/Headers/tcl-private"
	$(_v) $(MV) $(foreach f,$(PrivateHeaderItems),"$(DSTROOT)$(FmwkDir)/PrivateHeaders/$(f)") "$(DSTROOT)$(FmwkDir)/Headers/tcl-private"
	$(_v) $(RM) -rf "$(DSTROOT)$(FmwkDir)/PrivateHeaders"
	$(_v) $(LN) -fs "Headers/tcl-private" "$(DSTROOT)$(FmwkDir)/PrivateHeaders"

links:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRLIBDIR)"
	$(_v) $(LN) -fs "../..$(FmwkDir)/Tcl" "$(DSTROOT)/usr/lib/libtcl$(Version).dylib"
	$(_v) $(LN) -fs "libtcl$(Version).dylib" "$(DSTROOT)$(USRLIBDIR)/libtcl.dylib"
	$(_v) $(LN) -fs $(foreach f,$(LibItems),"../..$(FmwkDir)/$(f)") "$(DSTROOT)/usr/lib"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRINCLUDEDIR)"
	$(_v) $(LN) -fs $(foreach f,$(HeaderItems),"../..$(FmwkDir)/Headers/$(f)") \
	        "$(DSTROOT)/usr/include"
# Install compat sym links for private headers in /usr/local/include (3198305)
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/usr/local/include"
	$(_v) $(LN) -fs $(foreach f,$(PrivateHeaderItems),"../../..$(FmwkDir)/Headers/tcl-private/$(f)") \
	        "$(DSTROOT)/usr/local/include/"
# Move tclsh.1 to tclsh$(Version).1 and
# link tclsh.1 to tclsh$(Version).1 (2853545)
	$(_v) $(MV) "$(DSTROOT)$(MANDIR)/man1/tclsh.1$(Manpage_Suffix)" "$(DSTROOT)$(MANDIR)/man1/tclsh$(Version).1$(Manpage_Suffix)"
	$(_v) $(LN) -fs "tclsh$(Version).1$(Manpage_Suffix)" "$(DSTROOT)$(MANDIR)/man1/tclsh.1$(Manpage_Suffix)"

###

Sources               = $(SRCROOT)/$(Project)
MakeDir               = $(Sources)/macosx

Fmwk                  = $(SYMROOT)/Tcl.framework
BuildTarget           = deploy

all: build

.PHONY: almostclean $(AfterInstall)

build:: $(Fmwk)

$(Fmwk):
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) $(BuildTarget)

install:: $(Fmwk)
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(MakeDir) $(Environment) \
	        $(Install_Flags) install-$(BuildTarget)
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(MAKE) $(AfterInstall)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT)
	SYMROOT="$(SYMROOT)" DSTROOT="$(DSTROOT)" $(SRCROOT)/fix_tclConfig.sh

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(MAKE) -C $(MakeDir) $(Environment) clean-$(BuildTarget)

lipo64:
ifeq '$(RC_x86_64)' 'YES'
	$(_v) lipo -remove x86_64 -output $(DSTROOT)/usr/bin/tclsh$(Version) \
		$(DSTROOT)/usr/bin/tclsh$(Version)
endif
ifeq '$(RC_ppc64)' 'YES'
	$(_v) lipo -remove ppc64 -output $(DSTROOT)/usr/bin/tclsh$(Version) \
		$(DSTROOT)/usr/bin/tclsh$(Version)
endif
