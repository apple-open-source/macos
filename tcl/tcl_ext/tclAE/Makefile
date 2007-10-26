##
# Makefile for tclAE
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = TclAE

Extra_Environment     = GCC_VERSION=4.0 PRECOMPILE_PREFIX_HEADER=NO \
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
#	~fs/bin/xattrutil remove -a $(DSTROOT)/System/Library/Tcl/tclAE2.0
	mkdir $(DSTROOT)/System/Library/Tcl/tclAE2.0.foo
	mv $(DSTROOT)/System/Library/Tcl/tclAE2.0/* $(DSTROOT)/System/Library/Tcl/tclAE2.0.foo
	rm -rf $(DSTROOT)/System/Library/Tcl/tclAE2.0
	mv $(DSTROOT)/System/Library/Tcl/tclAE2.0.foo $(DSTROOT)/System/Library/Tcl/tclAE2.0
