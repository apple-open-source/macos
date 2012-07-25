##
# Makefile for thread
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = thread
ProjectName           = Thread

# Fetch info
Release               = 2.6.6
TEApotVersion         = 0.0.0.2010.08.18.09.07.53
TEApotProject         = $(Project)
#UrlFile               = $(Project)$(Release)
#Url                   = $(UrlBase)/tcl/Thread%20Extension/$(Release)/$(UrlFile)$(UrlExt)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc
