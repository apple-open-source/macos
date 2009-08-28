##
# Makefile for ffidl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = ffidl
ProjectName           = Ffidl

# Fetch info
Release               = 0.6.1
Url                   = http://www.categorifiedcoder.info/tcltk/$(Project)/$(UrlFile)-darwin-9-univ$(UrlExt)

Configure_Products    = ffidlConfig.h
Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = $(ProjectName)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtHtmlDir)/images"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.html "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/images/*.{gif,jpg} "$(DSTROOT)/$(TclExtHtmlDir)/images"
