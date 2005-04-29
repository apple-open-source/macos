##
# Makefile for tktable
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tktable
ProjectName           = TkTable

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_LD_Flags        = -L$(TclFramework) -L$(TkFramework)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = Tktable

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc/tkTable.html" "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc/tkTable.n" "$(DSTROOT)/$(TclExtManDir)"
