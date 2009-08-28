##
# Makefile for tbcload
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tbcload

# Fetch info
Release               = 1.7
Url                   = http://www.categorifiedcoder.info/tcltk/$(UrlFile)$(UrlExt)
ExtractedDir          = $(Project)_src

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries
