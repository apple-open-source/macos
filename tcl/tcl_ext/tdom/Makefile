##
# Makefile for tdom
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tdom
ProjectName           = tDOM
License               = LICENSE

# Fetch info
Release               = 0.8.3
TEApotVersion         = 0.0.0.2008.08.27.18.17.27
TEApotProject         = $(Project)
#UrlFile               = $(ProjectName)-$(Release)
#Url                   = http://www.tdom.org/files/$(UrlFile).tgz

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-config fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

# the tnc subproject is built by a separate Makefile
build install clean almostclean::
	$(_v) $(MAKE) -f Makefile.tnc $@ \
	    Extra_TEA_Configure_Flags="--with-tdom=$(OBJROOT)" \
	    OBJROOT="$(OBJROOT)/tnc" DSTROOT="$(DSTROOT)"
