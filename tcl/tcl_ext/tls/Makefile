##
# Makefile for tls
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tls
ProjectName           = TLS

# Fetch info
Release               = 1.6
TEApotVersion         = 0.0.0.2008.06.18.23.14.00
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
