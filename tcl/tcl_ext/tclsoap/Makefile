##
# Makefile for tclsoap
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclsoap
ProjectName           = TclSOAP
License               = LICENSE

# Fetch info
Release               = 1.6.7
ImportDate            = 2008-02-29
CvsRoot               = :pserver:anonymous@$(Project).cvs.sourceforge.net:/cvsroot/$(Project)
CvsTag                = soap1_6_7_as

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{html,css,gif} \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
