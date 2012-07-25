##
# Makefile for tclresource
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclresource
ProjectName           = TclResource
ProjectDoc            = tclresource-doc
License               = doc/license.terms

# Fetch info
Release               = 1.1.2
TEApotVersion         = 0.0.0.2007.11.22.23.13.59
TEApotProject         = resource-source

ifeq ($(Project),$(ProjectDoc))
TEApotVersion         = 0.0.0.2007.11.22.23.13.52
TEApotProject         = resource-help
Configure             = :
endif

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = Tclresource

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

ifneq ($(Project),$(ProjectDoc))
fetch extract wipe::
	$(_v) $(MAKE) $@ Project=$(ProjectDoc)

extract:: $(SRCROOT)/$(Project)/doc
$(SRCROOT)/$(Project)/doc:
	$(_v) $(MV) $(SRCROOT)/$(ProjectDoc) $@ && touch $(SRCROOT)/$(ProjectDoc)
endif
