##
# Makefile for tclx
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclx
ProjectName           = Tclx

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CPP_Flags       = -I$(TclHeaders) -I$(TclPrivateHeaders)
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = CPPFLAGS="$(CPPFLAGS)" $(TEA_Environment)

GnuAfterInstall       = strip

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc
