##
# Makefile for tklib
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tklib
ProjectName           = Tklib

# Fetch info
Release               = 0.5
ImportDate            = 2010-09-03
CvsRoot               = :pserver:anonymous@tcllib.cvs.sourceforge.net:/cvsroot/tcllib
#TEApotVersion         = 0.0.0.2010.08.18.09.08.14
#TEApotProject         = $(Project)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-libraries

install-doc:
	$(_v) TCLLIBPATH="$(TCLLIBPATH)" \
	        $(Tclsh) $(Sources)/installer.tcl \
                -nroff-path "$(DSTROOT)/$(TclExtManDir)" \
                -no-examples -no-pkgs -no-wait -no-gui -no-apps
