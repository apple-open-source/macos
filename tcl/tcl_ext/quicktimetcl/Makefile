##
# Makefile for quicktimetcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = quicktimetcl
ProjectName           = QuickTimeTcl
License               = README

# Fetch info
Release               = 3.2
TEApotVersion         = 0.0.0.2008.05.06.23.05.17
ExtractOptions        = -aa

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = $(ProjectName)

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/docs"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"

extract::
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/{bins,macos,pkgIndex.tcl}
	$(_v) $(CP) $(SRCROOT)/../tclconfig/install-sh $(SRCROOT)/$(Project)/tclconfig
