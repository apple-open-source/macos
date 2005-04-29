##
# Makefile for thread
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = thread
ProjectName           = Thread

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS=

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/html/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
