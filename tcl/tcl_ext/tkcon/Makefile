##
# Makefile for tkcon
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tkcon
License               = docs/license.terms

# Fetch info
Release               = 2.6
TEApotVersion         = 0.0.0.2010.08.18.09.08.05

Configure             = :

GnuAfterInstall       = install-doc fix-perms
GnuNoBuild            = YES
GnuNoInstall          = YES
GnuNoClean            = YES

install::
	@echo "Installing $(Project)..."
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/*.tcl "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRBINDIR)"
	$(_v) $(LN) -fs "../..$(TclExtLibDir)/$(TclExtDir)$(Version)/$(ProjectName).tcl" "$(DSTROOT)$(USRBINDIR)/$(ProjectName)"

# include TEA Tcl extension build support
include ../TEA.make

Version               = $(shell awk '/^package ifneeded/ {print $$4}' "$(Sources)"/pkgIndex.tcl)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(MANDIR)/man"{1,5,n}
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/docs/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n" && \
	    $(MPEXPAND) nroff "$$f" "$(DSTROOT)$(MANDIR)/man$${n##*.}/$$n"; \
	done

fix-perms::
	$(_v)- $(CHMOD) $(Install_Program_Mode) \
	    "$(DSTROOT)$(TclExtLibDir)/$(TclExtDir)$(Version)/$(ProjectName).tcl"
