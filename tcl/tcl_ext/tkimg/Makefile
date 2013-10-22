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
ImportDate            = 2010-08-31
SvnUrl                = https://$(Project).svn.sourceforge.net/svnroot/$(Project)/trunk

#TEApotVersion         = 0.0.0.2008.12.02.10.57.38
#TEApotProject         = Img

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
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(TclExtManDir)"
	$(_v) umask $(Install_Mask); for f in "$(Sources)"/doc/*.man; do \
	    n=$$(basename "$$f" .man) && echo "Generating $$n docs" && \
	    $(MPEXPAND) nroff $$f "$(DSTROOT)/$(TclExtManDir)/$$n.n"; \
	done

# 13302200: #include_next doesn't work right if the C source is in the same
# directory as the header.  So we symlink the source into a src subdirectory,
# and fix VPATH to find the source there (we still need the source in the
# original directory for configure to find it).  When 13302200 is fixed, we
# can remove the symlinking and VPATH change.
extract::
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/compat/zlib
	$(_v) $(RM) $(SRCROOT)/$(Project)/compat/libjpeg/ansi2knr.c
	$(_v) $(RMDIR) $(SRCROOT)/$(Project)/compat/libpng/contrib/gregbook
	$(_v) $(MKDIR) $(SRCROOT)/$(Project)/zlib/src
	@set -x && \
	cd $(SRCROOT)/$(Project)/zlib && \
	for i in *.c; do \
	    $(LN) -s ../$$i src/$$i || exit 1; \
	done
