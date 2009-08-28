##
# Makefile for tclae
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclae
ProjectName           = TclAE

# Fetch info
Release               = 2.0.3
TEApotVersion         = 0.0.0.2008.01.10.23.11.30
TEApotProject         = tclAE

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = tclAE

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{html,css,png} "$(DSTROOT)/$(TclExtHtmlDir)"
