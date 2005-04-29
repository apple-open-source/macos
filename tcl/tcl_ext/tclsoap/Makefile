##
# Makefile for tclsoap
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclsoap
ProjectName           = TclSOAP

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{html,css,gif} \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
