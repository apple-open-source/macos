##
# Makefile for tclapplescript
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclapplescript
ProjectName           = TclAppleScript

Extra_Environment     = FRAMEWORK_SEARCH_PATHS="$(shell dirname $(TclFramework))" \
                        LIBRARY_SEARCH_PATHS="$(TclFramework)" $(TEA_Environment)
Extra_Install_Flags   = INSTALL_PATH='$(TclExtLibDir)/$(ProjectName)$${DYLIB_CURRENT_VERSION}'

AfterInstall          = install-doc fix-owner

TEA_UseXcode          = YES

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(MV) -f "$(DSTROOT)/$(TclExtLibDir)/$(ProjectName)"*/*.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
