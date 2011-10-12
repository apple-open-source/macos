#
# Copyright (c) 2008-2011 Apple Inc. All Rights Reserved.
#

# General project info
Project         = libpng
UserType        = Developer
ToolType        = Library

# Some Makefile features are available only on Snow Leopard and later.
_OS_VERSION = $(shell sw_vers -productVersion | cut -d. -f1-2)

# Project-specific additions.
#Extra_Environment	= 
Extra_Configure_Flags	= --enable-static --disable-shared
Extra_Make_Flags        =

# Required or AEP parts won't be picked up.
GnuAfterInstall		= install-macosx

# Override defaults because we do want this in /usr/local...
Install_Prefix	= $(USRDIR)/local
Install_Man	= $(Install_Prefix)/share/man
Install_Info	= $(Install_Prefix)/info

# Automatic Extract & Patch
AEP			= YES
AEP_Version		= 1.5.4
# License file must be empty as this is a subproject.
AEP_LicenseFile		=
AEP_Patches		=


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

DESTDIR	= $(DSTROOT)

# This needs to be overridden because the project properly uses DESTDIR.
Install_Flags	= DESTDIR="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install

install-macosx:
	@echo "Reorganizing install for Mac OS X..."
	$(RMDIR) $(DSTROOT)/usr/local/share
	$(RMDIR) $(DSTROOT)/usr/local/bin
