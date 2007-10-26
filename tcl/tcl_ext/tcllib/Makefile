##
# Makefile for tcllib
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tcllib
ProjectName           = Tcllib

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-libraries

install-doc:
	$(_v) $(Tclsh) $(Sources)/installer.tcl \
                -nroff-path "$(DSTROOT)/$(TclExtManDir)" \
                -html-path  "$(DSTROOT)/$(TclExtHtmlDir)" \
                -no-examples -no-pkgs -no-wait -no-gui -no-apps
