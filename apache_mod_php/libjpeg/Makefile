##
# Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
##
#

# General project info
Project         = libjpeg
UserType        = Developer
ToolType        = Library

# Some Makefile features are available only on Snow Leopard and later.
_OS_VERSION = $(shell sw_vers -productVersion | cut -d. -f1-2)

# Project-specific additions.
#Extra_Environment	= 
Extra_Configure_Flags	= --enable-static --disable-shared
Extra_Make_Flags		=
# Required or AEP parts won't be picked up.
GnuAfterInstall		= install-macosx

# Override defaults because we do want this in /usr/local...
Install_Prefix	= $(USRDIR)/local
Install_Man	= $(Install_Prefix)/share/man
Install_Info	= $(Install_Prefix)/info

# Automatic Extract & Patch
AEP		= YES
AEP_Version	= 8c
AEP_Filename	= jpegsrc.v$(AEP_Version).tar.gz
AEP_ExtractDir	= jpeg-$(AEP_Version)
AEP_Patches	=
# License file must be empty as this is a subproject.
AEP_LicenseFile	=


# Defines only used here.

# Local targets that must be defined before including the following
# files to get the dependency order correct
install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
ifeq ($(_OS_VERSION),10.6)
include $(SRCROOT)/../AEP.make
else
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make
endif

#
# Override settings from above include
#
DESTDIR	= $(DSTROOT)

# This needs to be overridden because the project properly uses DESTDIR.
Install_Flags	= DESTDIR="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install

# Done with overrides

install-macosx:
	@echo "No additional work necessary for Mac OS X."
