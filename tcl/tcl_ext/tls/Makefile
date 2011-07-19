##
# Makefile for tls
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tls
ProjectName           = TLS

# Fetch info
Release               = 1.6.1
TEApotVersion         = 0.0.0.2010.08.18.09.08.25
TEApotProject         = $(Project)

Extra_Configure_Flags = $(TEA_Configure_Flags) --with-ssl-dir=$(USRDIR)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/$(Project).htm \
	        "$(DSTROOT)/$(TclExtHtmlDir)"/$(Project).html
