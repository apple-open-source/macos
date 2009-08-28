##
# Makefile for trf
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = trf
ProjectName           = Trf
License               = doc/license.terms

# Fetch info
Release               = 2.1.3
#TEApotVersion         = $(Release).2008.12.05.14.50.18
ImportDate            = 2008-12-12
CvsRoot               = :pserver:anonymous@tcltrf.cvs.sourceforge.net:/cvsroot/tcltrf

Extra_Configure_Flags = $(TEA_Configure_Flags) \
                        --with-zlibtcl=$(OBJROOT)/../tkimg/libz/tcl
                        $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = $(ProjectName)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) html $$f "$(DSTROOT)/$(TclExtHtmlDir)/$$n.html" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done

extract::
	$(_v) $(RM) $(SRCROOT)/$(Project)/compat/tclLoadDyld.c
