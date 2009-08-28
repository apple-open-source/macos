##
# Makefile for tkimg
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = tkimg
ProjectName           = TkImg

# Fetch info
Release               = 1.4
TEApotVersion         = 0.0.0.2008.12.02.10.57.38
TEApotProject         = Img

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment) CONFIG_SITE=
Extra_Install_Flags   = PKG_HEADERS= INSTALL_ROOT=$(DSTROOT)

GnuAfterInstall       = fix-config strip install-doc fix-perms
GnuNoBuild            = YES

# include TEA Tcl extension build support
include ../TEA.make

TclExtStubConfig      = *Config.sh 
TclExtDir             = Img

build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)

install-doc:
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtHtmlDir)" "$(DSTROOT)/$(TclExtManDir)"
	$(_v) $(INSTALL_FILE) "$(Sources)"/doc/*.{htm,css} "$(DSTROOT)/$(TclExtHtmlDir)"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) html $$f "$(DSTROOT)/$(TclExtHtmlDir)/$$n.html" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done

extract::
	$(_v) $(RM) $(SRCROOT)/$(Project)/libtiff/libtiff/{tif_config.h,tiffconf.h}
	$(_v) $(MV) $(SRCROOT)/$(Project)/libz/tcl $(SRCROOT)/$(Project)/libz-tcl
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/libz/*
	$(_v) $(MV) $(SRCROOT)/$(Project)/libz-tcl $(SRCROOT)/$(Project)/libz/tcl
