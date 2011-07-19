##
# Makefile for incrtcl
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

# Project info
Project               = incrTcl
ProjectPlistName      = incrtcl

# Fetch info
Release               = 3.4
ImportDate            = 2010-08-18
CvsRoot               = :pserver:anonymous@$(ProjectPlistName).cvs.sourceforge.net:/cvsroot/$(ProjectPlistName)

Extra_Configure_Flags = $(TEA_Configure_Flags)
Extra_Environment     = $(TEA_Environment)

GnuNoBuild            = YES
ifneq ($(PureTclExt),YES)
GnuAfterInstall       = strip fix-config fix-perms
else
GnuNoInstall          = YES
GnuNoClean            = YES
Configure             = :
endif

# include TEA Tcl extension build support
include ../TEA.make

Install_Target        = install-binaries install-doc

TclExtDir             = {itcl,itk}
TclExtStubConfig      = itclConfig.sh

ifneq ($(PureTclExt),YES)
build::
	@echo "Building $(Project)..."
	$(_v) $(Environment) $(MAKE) -C $(BuildDirectory)
endif

ifneq ($(PureTclExt),NO)
# the iwidgets subproject is built by a separate Makefile
build install clean almostclean $(fetch_targets)::
	$(_v) $(MAKE) -f Makefile.iwidgets $@ \
	    Extra_TEA_Configure_Flags="--with-itcl=$(Sources)" \
	    OBJROOT="$(OBJROOT)/iwidgets" DSTROOT="$(DSTROOT)"
endif
