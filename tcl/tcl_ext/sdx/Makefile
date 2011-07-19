##
# Makefile for sdx
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = sdx
License               = ../license.terms

# Fetch info
Release               = 2.0
ImportDate            = 2010-08-30
SvnUrl                = http://tclkit.googlecode.com/svn/trunk/$(Project)

Configure             = :

GnuAfterInstall       = install-doc
GnuNoBuild            = YES
GnuNoInstall          = YES
GnuNoClean            = YES

SDX                   = TCLLIBPATH='$(DSTROOT)$(TclExtLibDir)' $(Tclsh) "$(Sources)/main.tcl"

install:: build
	@echo "Installing $(Project)..."
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(TclExtLibDir)/bin"
	$(_v) $(INSTALL_SCRIPT) "$(BuildDirectory)/$(ProjectName)" "$(DSTROOT)$(TclExtLibDir)/bin"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(USRBINDIR)"
	$(_v) $(LN) -fs "../..$(TclExtLibDir)/bin/$(ProjectName)" "$(DSTROOT)$(USRBINDIR)/"

build:: configure
	@echo "Building $(Project)..."
	$(_v) cd "$(BuildDirectory)" && \
	    $(SDX) wrap "$(ProjectName)" -vfs "$(Sources)" -interp tclsh

almostclean::
	@echo "Cleaning $(Project)..."
	$(_v) $(RM) "$(BuildDirectory)/$(ProjectName)"

# include TEA Tcl extension build support
include ../TEA.make

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(MANDIR)/man1"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/../*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n" && \
	    $(MPEXPAND) nroff "$$f" "$(DSTROOT)$(MANDIR)/man$${n##*.}/$$n"; \
	done

extract::
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/{ChangeLog,doc,lib/{app-sdx/crc16.tcl,autoscroll,gbutton,wikit}}
