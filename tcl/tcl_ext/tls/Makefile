##
# Makefile for tls
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tls
ProjectName           = TLS

Extra_Configure_Flags = $(TEA_Configure_Flags) --with-ssl-dir=$(USRDIR)
Extra_CC_Flags        = -fno-common
Extra_LD_Flags        = -L$(TclFramework)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/$(Project).htm \
	        "$(DSTROOT)/$(TclExtHtmlDir)"/$(Project).html
