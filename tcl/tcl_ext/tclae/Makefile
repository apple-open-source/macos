##
# Makefile for tclae
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclae
ProjectName           = TclAE

# Fetch info
Release               = 2.0.5
ImportDate            = 2010-08-02
SvnUrl                = https://$(Project).svn.sourceforge.net/svnroot/$(Project)/trunk
#TEApotVersion         = 0.0.0.2008.01.10.23.11.30
#TEApotProject         = tclAE

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = tclAE

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)
