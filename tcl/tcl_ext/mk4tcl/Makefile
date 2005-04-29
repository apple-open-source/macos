##
# Makefile for mk4tcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = metakit
ProjectName           = Mk4Tcl

Configure             = $(Sources)/unix/configure
Extra_Configure_Flags = $(TEA_Configure_Flags) \
                        --with-tcl=$(TclHeaders),$(TclExtLibDir)
Extra_Environment     = CC=g++ $(TEA_Environment)

GnuAfterInstall       = install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-tcl

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/tcl.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)/$(ProjectName).html"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.gif "$(DSTROOT)/$(TclExtHtmlDir)"
