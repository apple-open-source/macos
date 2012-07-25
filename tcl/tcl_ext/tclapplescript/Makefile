##
# Makefile for tclapplescript
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclapplescript
ProjectName           = TclAppleScript

# Fetch info
Release               = 1.0
TEApotVersion         = 0.0.0.2007.11.22.23.07.37
TEApotProject         = Tclapplescript

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = Tclapplescript

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)
