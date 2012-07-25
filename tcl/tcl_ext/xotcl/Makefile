##
# Makefile for xotcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = xotcl
ProjectName           = XOTcl
License               = COPYRIGHT

# Fetch info
Release               = 1.6.6
Url                   = http://media.wu-wien.ac.at/download/$(UrlFile)$(UrlExt)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)
Extra_Install_Flags   = PKG_HEADERS=

GnuAfterInstall       = strip fix-config fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-libraries

fix-perms::
	$(_v)- $(CHMOD) $(Install_Program_Mode) \
	    "$(DSTROOT)/$(TclExtLibDir)"/$(TclExtDir)*/apps/{utils/*,comm/*.xotcl}

extract::
	$(_v) $(RM) $(SRCROOT)/$(Project)/{ChangeLog.*,doc/{index.html,xo-daemon.html,xo-whichPkg.html}}
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/doc/openacs-4
