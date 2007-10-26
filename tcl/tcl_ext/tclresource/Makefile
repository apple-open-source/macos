##
# Makefile for tclresource
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclresource
ProjectName           = tclResource

Extra_Environment     = FRAMEWORK_SEARCH_PATHS="$(shell dirname $(TclFramework))" \
                        LIBRARY_SEARCH_PATHS="$(TclFramework)" $(TEA_Environment)
Extra_Install_Flags   = INSTALL_PATH='$(TclExtLibDir)/$(ProjectName)$${DYLIB_CURRENT_VERSION}'

AfterInstall          = install-doc fix-owner

TEA_UseXcode          = YES

# include TEA Tcl extension build support
include ../TEA.make

Sources               = $(SRCROOT)/$(Project)/Source
Docs                  = $(SRCROOT)/$(Project)/Help

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Docs)"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Docs)"/$(Project).n "$(DSTROOT)/$(TclExtManDir)"
