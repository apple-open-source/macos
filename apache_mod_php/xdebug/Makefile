#
# Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
#

# General project info
Project         = xdebug
UserType        = Developer
ToolType        = Library

# Some Makefile features are available only on Snow Leopard and later.
_OS_VERSION = $(shell sw_vers -productVersion | cut -d. -f1-2)

# Project-specific additions.
Extra_Environment	= INSTALL_ROOT="$(DSTROOT)"
Extra_Configure_Flags	= --enable-xdebug
Extra_Make_Flags        =

# Required or AEP parts won't be picked up.
GnuAfterInstall		= install-macosx

# Override defaults because we do want this in /usr/local...
Install_Prefix	= $(USRDIR)/local
Install_Man	= $(Install_Prefix)/share/man
Install_Info	= $(Install_Prefix)/info

# Automatic Extract & Patch
AEP			= YES
AEP_Version		= 2.1.1
AEP_Binaries		= $(shell $(DSTROOT)$(USRBINDIR)/php-config --extension-dir)/$(Project).so
# License file must be empty as this is a subproject.
AEP_LicenseFile		=
AEP_Patches		=


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: post-extract-source $(GnuAfterInstall)

install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
ifeq ($(_OS_VERSION),10.6)
include $(SRCROOT)/../AEP.make
else
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make
endif

DESTDIR	= $(DSTROOT)

# This needs to be overridden because the project properly uses DESTDIR.
Install_Flags	= DESTDIR="$(DSTROOT)" INSTALL_ROOT="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install

# Dependency info necessary to ensure temp directory and cleanup are performed.
$(GNUConfigStamp): post-extract-source

post-extract-source: extract-source
	@echo "Creating configure script..."
	cd $(Sources) && $(DSTROOT)$(USRBINDIR)/phpize

install-macosx:
	@echo "Archiving binaries..."
	$(_v) for file in $(addprefix $(DSTROOT),$(AEP_Binaries));	\
	do \
		$(CP) $${file} $(SYMROOT);				\
		$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
	done
