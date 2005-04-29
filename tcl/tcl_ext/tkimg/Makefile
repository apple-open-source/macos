##
# Makefile for tkimg
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tkimg
ProjectName           = TkImg

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_CC_Flags        = -fno-common
Extra_LD_Flags        = -L$(TclFramework) -L$(TkFramework)
Extra_Environment     = $(TEA_Environment)
Extra_Install_Flags   = GENERIC_HDRS= INSTALL_ROOT=$(DSTROOT)

GnuAfterInstall       = fix-config strip install-doc fix-owner

# include TEA Tcl extension build support
include ../TEA.make

TclExtStubConfig      = *Config.sh 
TclExtDir             = Img

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{htm,css} "$(DSTROOT)/$(TclExtHtmlDir)"
ifdef MPEXPAND
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    echo "Generating $$(basename $$f .man) docs" && \
	    $(MPEXPAND) html $$f "$(DSTROOT)/$(TclExtHtmlDir)"/$$(basename $$f .man).html && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)"/$$(basename $$f .man).n ; \
	    done
endif
