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

Extra_Configure_Flags = $(TEA_Configure_Flags) --with-ssl-dir=$(SDKROOT)$(USRDIR)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries
