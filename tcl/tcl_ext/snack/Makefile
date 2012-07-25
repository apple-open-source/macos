##
# Makefile for snack
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = snack
ProjectName           = Snack
License               = COPYING

# Fetch info
Release               = 2.2.10
UrlFile               = $(Project)$(Release)
Url                   = http://www.speech.kth.se/$(Project)/dist/$(UrlFile)$(UrlExt)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

extract::
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/mac
	$(_v) $(CP) $(SRCROOT)/../tclconfig/install-sh $(SRCROOT)/$(Project)/tclconfig
