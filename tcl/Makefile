##
# Makefile for Tcl
##

# Project info
Project               = tcl
UserType              = Developer
ToolType              = Commands
Configure             = $(Sources)/unix/configure
Extra_Configure_Flags = --includedir="$(USRDIR)/local/include"
Extra_Environment     = TCL_LIBRARY="$(NSLIBRARYDIR)/Tcl/$(Version)"	\
			TCL_EXE="$(Tclsh)"
GnuAfterInstall       = links

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags = INSTALL_ROOT="$(DSTROOT)" INSTALL_STRIP_LIBRARY="-S -S"

Version = $(shell $(GREP) "TCL_VERSION=" "$(Configure)" | $(CUT) -d '=' -f 2)
Tclsh   = $(shell find /usr/bin -name tcl\* | tail -1)

links:
	$(_v) $(LN) -fs "tclsh$(Version)" "$(DSTROOT)$(USRBINDIR)/tclsh"
	$(_v) $(LN) -fs "libtcl$(Version).dylib" "$(DSTROOT)$(USRLIBDIR)/libtcl.dylib"
