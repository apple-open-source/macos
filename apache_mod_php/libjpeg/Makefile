##
# Copyright (c) 2008-2009 Apple Inc. All Rights Reserved.
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
# The following two flags are defined in the system Makefiles only for Snow Leopard.
ifeq ($(_OS_VERSION),10.6)
Extra_Make_Flags		=
else
Extra_Configure_Environment	= $(Environment)
endif
# Required or AEP parts won't be picked up.
GnuAfterInstall		= install-macosx

# Override defaults because we do want this in /usr/local...
Install_Prefix	= $(USRDIR)/local
Install_Man	= $(Install_Prefix)/share/man
Install_Info	= $(Install_Prefix)/info

# Automatic Extract & Patch
AEP		= YES
AEP_Version	= 6b
AEP_Filename	= jpegsrc.v$(AEP_Version).tar.gz
AEP_ExtractDir	= jpeg-$(AEP_Version)
AEP_Patches	= makefile.cfg.patch
# License file must be empty as this is a subproject.
AEP_LicenseFile	=


# Defines only used here.
# The needed files moved locations between Leopard and Snow Leopard, so this
# will catch wherever they are on this build.
LibtoolBits		= $(wildcard /usr/share/libtool/config/config.* /usr/share/libtool/config.*)

# Local targets that must be defined before including the following
# files to get the dependency order correct
install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(SRCROOT)/../AEP.make

#
# Override settings from above include
#
ifndef MACOSX_DEPLOYMENT_TARGET
	MACOSX_DEPLOYMENT_TARGET = $(shell sw_vers -productVersion | cut -d. -f1-2)
endif
DESTDIR	= $(DSTROOT)

# This needs to be overridden because the project properly uses DESTDIR.
Install_Flags	= DESTDIR="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install-lib
ifneq ($(_OS_VERSION),10.6)
# Since Environment is passed to BOTH configure AND make (overriding what may have
# been defined by configure), clear the default Environment since it was already saved
# for use with configure by defining Extra_Configure_Environment.
Environment			=
endif

# Done with overrides


$(GNUConfigStamp): copy-libtool-bits

# After source extraction, need to copy libtool bits into place.
copy-libtool-bits: extract-source
	@echo "Copying libtool bits..."
	$(CP) -f $(LibtoolBits) $(Sources)

install-macosx:
	@echo "No additional work necessary for Mac OS X."

# Custom configuration (historical, not used)
do_configure: 
	@$(ECHO) "Configuring $(PROJECT_NAME)..."
	$(CD) $(PROJECT_DIR); \
			./configure \
			LDFLAGS="$$RC_CFLAGS -Os" \
			CFLAGS="$$RC_CFLAGS -Os" \
			MACOSX_DEPLOYMENT_TARGET=10.6 \
			--enable-static \
			--disable-dependency-tracking
