##
# Makefile for bwidget
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = bwidget
License               = LICENSE.txt

# Fetch info
Release               = 1.8.0
TEApotVersion         = 0.0.0.2008.10.30.19.39.59
TEApotProject         = Bwidget

Configure             = :

GnuAfterInstall       = install-doc fix-perms
GnuNoBuild            = YES
GnuNoInstall          = YES
GnuNoClean            = YES

install::
	@echo "Installing $(Project)..."
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"/{,images,lang}
	$(_v) $(INSTALL_FILE) "$(Sources)"/*.tcl "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/images/*.{xbm,gif} "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"/images
	$(_v) $(INSTALL_FILE) "$(Sources)"/lang/*.rc "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"/lang

# include TEA Tcl extension build support
include ../TEA.make

Version               = $(shell awk '/^package ifneeded/ {print $$4}' "$(Sources)"/pkgIndex.tcl)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(TclExtHtmlDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/BWman/*.html "$(DSTROOT)$(TclExtHtmlDir)"
