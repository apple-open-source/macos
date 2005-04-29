##
# Makefile for tclmemchan
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = memchan
ProjectName           = Memchan

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CC_Flags        = -fno-common
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = CC=gcc $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

TclExtDir             = $(ProjectName)

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment) doc-html

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(BuildDirectory)"/*.html \
	        "$(DSTROOT)/$(TclExtHtmlDir)"
