##
# Makefile for tcllib
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tcllib
ProjectName           = Tcllib

# Fetch info
Release               = 1.12
ImportDate            = 2010-08-30
CvsRoot               = :pserver:anonymous@$(Project).cvs.sourceforge.net:/cvsroot/$(Project)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-libraries

install-doc:
	$(_v) $(Tclsh) $(Sources)/installer.tcl \
                -nroff-path "$(DSTROOT)/$(TclExtManDir)" \
                -html-path  "$(DSTROOT)/$(TclExtHtmlDir)" \
                -no-examples -no-pkgs -no-wait -no-gui -no-apps

extract::
	$(_v) $(RM) $(SRCROOT)/$(Project)/examples/ftp/hpupdate.tcl
