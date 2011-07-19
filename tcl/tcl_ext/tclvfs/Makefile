##
# Makefile for tclvfs
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tclvfs
ProjectName           = TclVFS

# Fetch info
Release               = 1.4.1
TEApotVersion         = 0.0.0.2010.08.18.09.08.49
TEApotProject         = vfs

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuAfterInstall       = strip install-doc fix-perms

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries

TclExtDir             = vfs

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) html $$f "$(DSTROOT)/$(TclExtHtmlDir)/$$n.html" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done
