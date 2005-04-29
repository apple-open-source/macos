##
# Makefile for snack
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = snack
ProjectName           = Snack

Configure             = $(Sources)/unix/configure
Extra_CC_Flags        = -I$(TclHeaders) -I$(TkHeaders)
Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_LD_Flags        = -L$(TclFramework) -L$(TkFramework)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = fix-stub strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

fix-stub:
	$(_v) $(MV) "$(DSTROOT)/$(TclExtLibDir)"/lib$(Project)stub*.a \
	            "$(DSTROOT)/$(TclExtLibDir)"/$(Project)*
install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/doc"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(RM) "$(DSTROOT)/$(TclExtHtmlDir)"/{python*,Sphere*}

.PHONY: $(GnuAfterInstall)
