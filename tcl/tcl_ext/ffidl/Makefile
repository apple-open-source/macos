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
Extra_Configure_Flags = $(TEA_Configure_Flags) \
			--with-libffi-include=$(SDKROOT)$(USRINCLUDEDIR)/ffi \
			--with-libffi-lib=$(SDKROOT)$(USRLIBDIR)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = $(ProjectName)
