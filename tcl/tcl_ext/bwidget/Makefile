##
# Makefile for bwidget
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = bwidget

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-libraries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/BWman"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
