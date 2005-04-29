##
# Makefile for tktreectrl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tktreectrl
ProjectName           = TkTreeCtrl

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CPP_Flags       = -I$(TclHeaders) -I$(TclPrivateHeaders) \
                        -I$(TkHeaders) -I$(TkPrivateHeaders) 
Extra_LD_Flags        = -L$(TclFramework) -L$(TkFramework)
Extra_Environment     = CPPFLAGS="$(CPPFLAGS)" $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = treectrl

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc"/*.n "$(DSTROOT)/$(TclExtManDir)"
