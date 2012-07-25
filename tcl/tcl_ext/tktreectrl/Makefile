##
# Makefile for tktreectrl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tktreectrl
ProjectName           = TkTreeCtrl

# Fetch info
Release               = 2.2.10
Url                   = $(UrlBase)/$(Project)/$(Project)/$(UrlFile)/$(UrlFile)$(UrlExt)
#TEApotVersion         = 0.0.0.2010.08.18.09.08.36
#TEApotProject         = treectrl

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = treectrl

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done
