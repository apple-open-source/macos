##
# Makefile for quicktimetcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = quicktimetcl
ProjectName           = QuickTimeTcl

Extra_Environment     = TCL_FRAMEWORK="$(TclFramework)" TK_FRAMEWORK="$(TkFramework)" \
                        $(TEA_Environment)
Extra_Install_Flags   = INSTALL_PATH='$(TclExtLibDir)/$(ProjectName)$${DYLIB_CURRENT_VERSION}'

AfterInstall          = install-doc fix-owner

TEA_UseXcode          = YES

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)/docs"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
