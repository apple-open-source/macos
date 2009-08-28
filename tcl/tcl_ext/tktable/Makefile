##
# Makefile for tktable
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tktable
ProjectName           = TkTable
License               = license.txt

# Fetch info
Release               = 2.10
TEApotVersion         = 0.0.0.2008.11.14.18.16.40
TEApotProject         = Tktable

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries
TclExtDir             = Tktable

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc/tkTable.html" "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc/tkTable.n" "$(DSTROOT)/$(TclExtManDir)"
