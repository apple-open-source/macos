##
# Makefile for xotcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = xotcl
ProjectName           = XOTcl

Configure             = $(Sources)/unix/configure
Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CPP_Flags       = -I$(TclPrivateHeaders)
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = CPPFLAGS="$(CPPFLAGS)" $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS=

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-lib-binaries install-libraries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{html,gif,jpg,pdf} \
	    "$(DSTROOT)/$(TclExtHtmlDir)"

.PHONY: $(GnuAfterInstall)
