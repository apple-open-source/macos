##
# Makefile for tclx
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclx
ProjectName           = Tclx

# Fetch info
Release               = 8.4
TEApotVersion         = 0.0.0.2010.08.18.09.12.06

Extra_Configure_Flags = $(TEA_Configure_Flags) --with-help
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc
