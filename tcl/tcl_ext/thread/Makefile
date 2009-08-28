##
# Makefile for thread
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = thread
ProjectName           = Thread

# Fetch info
Release               = 2.6.5
TEApotVersion         = 0.0.0.2008.11.04.18.15.38
TEApotProject         = $(Project)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/html/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
