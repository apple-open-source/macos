##
# Makefile for tclAE
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = TclAE

Extra_Environment     = GCC_VERSION=3.x PRECOMPILE_PREFIX_HEADER=NO \
                        FRAMEWORK_SEARCH_PATHS="$(shell dirname $(TclFramework))" \
                        LIBRARY_SEARCH_PATHS="$(TclFramework)" $(TEA_Environment) \
                        INSTALL_ROOT="$(SYMROOT)/Deployment.build/"
Extra_Install_Flags   = INSTALL_ROOT="$(DSTROOT)$(TclExtLibDir)/"

AfterInstall          = install-doc fix-owner kill-appledouble

TEA_UseXcode          = YES

# include TEA Tcl extension build support
include ../TEA.make

Sources               = $(SRCROOT)/$(Project)/Build
Docs                  = $(SRCROOT)/$(Project)/Help/$(Project) Help

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)/style"
	$(_v) $(INSTALL_FILE) "$(Docs)"/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Docs)"/style/* "$(DSTROOT)/$(TclExtHtmlDir)"/style

kill-appledouble:
	-rm -f $(DSTROOT)/System/Library/Tcl/._tclAE2.0
