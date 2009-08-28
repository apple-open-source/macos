##
# Makefile for memchan
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = memchan
ProjectName           = Memchan
License               = doc/license.terms

# Fetch info
Release               = 2.2.1
TEApotVersion         = $(Release).2008.10.20.18.13.18

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc

TclExtDir             = $(ProjectName)

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) doc-html

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(BuildDirectory)"/*.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
