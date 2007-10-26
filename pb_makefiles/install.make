##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# install.make
#
# Rules for installing the product in its final destination.  Installation
# takes place in three stages.  The first stage is to install the
# products from the current project.  The next stage is to recursively perform
# post-install processing on every subproject.  Finally, the destination is
# chowned and chrooted.
#
# PUBLIC TARGETS
#    install: installs the product and any public or private header
#	 files into their final destination.
#
# IMPORTED VARIABLES
#    PRODUCTS: products to install.  All of these products will be placed in
#	 the directory $(DSTROOT)$(INSTALLDIR)
#    INSTALL_AS_USER: owner of the intalled products (default root)
#    INSTALL_AS_GROUP: group of the installed products (default wheel)
#    INSTALL_PERMISSIONS: permissions of the installed product (default o+rX)
#    BEFORE_INSTALL: targets to build before installing the product
#    AFTER_INSTALL: targets to build after installing the product
#    BEFORE_POSTINSTALL: targets to build before postinstalling every subproject
#    AFTER_POSTINSTALL: targts to build after postinstalling every subproject
#    INSTALLDIR: final destination for installed products
#

.PHONY:  install install-products install-hook
.PHONY:  postinstall recursive-postinstall local-postinstall

#
# Variable definitions
#

ifneq "YES" "$(BUILD_OFILES_LIST_ONLY)"
ACTUAL_INSTALL = install-products install-projtype-specific-products
ACTUAL_POSTINSTALL = strip-binaries install-headers
else
ACTUAL_INSTALL =
ACTUAL_POSTINSTALL = install-headers
endif
ifeq "$(OS)" "MACOS"
STRIPFLAGS = -S
endif
ifeq "$(OS)" "NEXTSTEP"
STRIPFLAGS = -S
endif
#
# RDW 04/15/1999 -- Changed the strip flag used on Solaris
#                   and HP-UX during install.
#
ifeq "$(PLATFORM_TYPE)" "PDO_UNIX"
STRIPFLAGS = -x
endif
ALL_STRIPFLAGS = $(STRIPFLAGS) $(PROJTYPE_STRIPFLAGS) $(OTHER_STRIPFLAGS)
INSTALLED_PRODUCTS = $(addprefix $(DSTROOT)$(INSTALLDIR)/, $(notdir $(PRODUCTS)))
ifeq "YES" "$(JAVA_USED)"
INSTALLED_JAVA_PRODUCT = $(JAVA_DSTROOT)$(JAVA_INSTALL_DIR)/$(JAVA_PRODUCT)
INSTALLED_JAVA_PRODUCT_CLIENT = $(JAVA_DSTROOT_CLIENT)$(JAVA_INSTALL_DIR_CLIENT)/$(JAVA_PRODUCT_CLIENT)
endif
ifeq "" "$(INSTALL_AS_USER)"
INSTALL_AS_USER = root
endif
ifeq "" "$(INSTALL_AS_GROUP)"
ifeq "MACOS" "$(OS)"
INSTALL_AS_GROUP = wheel
else
ifeq "NEXTSTEP" "$(OS)"
INSTALL_AS_GROUP = wheel
else
INSTALL_AS_GROUP = bin
endif
endif
endif
ifeq "" "$(INSTALL_PERMISSIONS)"
INSTALL_PERMISSIONS = o+rX
endif

#
# First we do local installation, then we recursively do postinstallation
#

ifneq "YES" "$(SUPPRESS_BUILD)"
ifeq ($(OS)-$(REINSTALLING)-$(STRIP_ON_INSTALL), WINDOWS--YES)
install: all
	      $(MAKE) reinstall-stripped REINSTALLING=YES
else
install: local-install postinstall finish-install
endif
postinstall: local-postinstall recursive-postinstall
recursive-postinstall: local-postinstall
recursive-postinstall: $(RECURSABLE_DIRS:%=postinstall@%)
ifndef RECURSING
postinstall local-postinstall recursive-postinstall: local-install
$(RECURSABLE_DIRS:%=postinstall@%): local-install
endif
endif

#
# Local installation
#

local-install: announce-install validate-install $(BEFORE_INSTALL) $(ACTUAL_INSTALL)
$(ACTUAL_INSTALL): announce-install validate-install $(BEFORE_INSTALL)
$(BEFORE_INSTALL): announce-install validate-install

local-postinstall: announce-postinstall $(BEFORE_POSTINSTALL) $(ACTUAL_POSTINSTALL) $(AFTER_POSTINSTALL)
$(AFTER_POSTINSTALL): announce-postinstall $(BEFORE_POSTINSTALL) $(ACTUAL_POSTINSTALL)
$(ACTUAL_POSTINSTALL): announce-postinstall $(BEFORE_POSTINSTALL)
$(BEFORE_POSTINSTALL): announce-postinstall

#
# before we do anything we announce our intentions
#

announce-install:
	$(SILENT) $(ECHO) Installing product...

announce-postinstall:
ifndef RECURSING
	$(SILENT) $(ECHO) Performing post-installation processing...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif

#
# We must ensure that we know where to put things
#

validate-install:
ifneq "YES" "$(BUILD_OFILES_LIST_ONLY)"
ifeq "" "$(INSTALLDIR)"
	$(SILENT) $(ECHO) install.make: error: INSTALLDIR variable not defined
	$(SILENT) exit 1
endif
ifeq "WINDOWS" "$(OS)"
ifneq "1" "$(words $(subst :, ,$(INSTALLDIR)))"
ifneq "" "$(DSTROOT)"
	$(SILENT) $(ECHO) install.make: error: DSTROOT specified and INSTALLDIR defines a drive letter
	$(SILENT) exit 1
endif
endif
endif
endif

#
# Actual installation is a matter of copying the product
# to the destination area
#

ifneq "" "$(INSTALLED_PRODUCTS)"
install-products: $(DSTROOT)$(INSTALLDIR)
	-$(CHMOD) -R +w $(INSTALLED_PRODUCTS)
	$(RM) -rf $(INSTALLED_PRODUCTS)
	($(CD) $(PRODUCT_DIR) && $(TAR) cf - $(notdir $(PRODUCTS))) | ($(CD) $(DSTROOT)$(INSTALLDIR) && $(TAR) xf -)
else
install-products:
endif

install-projtype-specific-products:

#
# after installing we must strip the binaries
#

ifneq "$(STRIPPED_PRODUCTS)" ""
ifdef STRIP
strip-binaries:
ifneq "$(STRIP_ON_INSTALL)" "NO"
	-$(STRIP) $(ALL_STRIPFLAGS) $(subst $(PRODUCT_DIR),$(DSTROOT)$(INSTALLDIR),$(STRIPPED_PRODUCTS))
ifneq "$(STRIPPED_PROFILED_PRODUCTS)" ""
	-$(SILENT) if [ -f $(subst $(PRODUCT_DIR),$(DSTROOT)$(INSTALLDIR),$(STRIPPED_PROFILED_PRODUCTS)) ] ; then \
	    cmd="$(STRIP) $(ALL_STRIPFLAGS) $(subst $(PRODUCT_DIR),$(DSTROOT)$(INSTALLDIR),$(STRIPPED_PROFILED_PRODUCTS))" ; \
	    $(ECHO) $$cmd ; eval $$cmd ; \
	fi
endif
endif
endif
endif

#
# To ensure that header files are correct, use the rules
# from installhdrs.make
#

install-headers: local-installhdrs

#
# then we finish installing and chmod/chown things
#

finish-install: change-permissions $(AFTER_INSTALL) change-java-permissions convert-bundle compress-man-pages
$(AFTER_INSTALL): change-permissions

ifneq "" "$(INSTALLED_PRODUCTS)"
change-permissions:
ifdef CHMOD
	-$(CHMOD) -R ugo-w $(INSTALLED_PRODUCTS)
	-$(CHMOD) -R $(INSTALL_PERMISSIONS) $(INSTALLED_PRODUCTS)
endif
ifdef CHGRP
	-$(CHGRP) -R $(INSTALL_AS_GROUP) $(INSTALLED_PRODUCTS)
endif
ifdef CHOWN
	-$(CHOWN) -R $(INSTALL_AS_USER) $(INSTALLED_PRODUCTS)
endif
else
change-permissions:
endif

change-java-permissions: change-java-perms change-java-perms-client

ifneq "" "$(INSTALLED_JAVA_PRODUCT)"
ifdef JAVA_DSTROOT
change-java-perms:
ifdef CHMOD
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR)" -a -d "$(JAVA_DSTROOT)$(JAVA_INSTALL_DIR)" ]; then \
	    $(ECHO) $(CHMOD) -R ugo-w $(INSTALLED_JAVA_PRODUCT); \
	    $(CHMOD) -R ugo-w $(INSTALLED_JAVA_PRODUCT); \
	    $(ECHO) $(CHMOD) -R $(INSTALL_PERMISSIONS) $(INSTALLED_JAVA_PRODUCT); \
	    $(CHMOD) -R $(INSTALL_PERMISSIONS) $(INSTALLED_JAVA_PRODUCT); \
	fi
endif
ifdef CHGRP
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR)" -a -d "$(JAVA_DSTROOT)$(JAVA_INSTALL_DIR)" ]; then \
	    $(ECHO) $(CHGRP) -R $(INSTALL_AS_GROUP) $(INSTALLED_JAVA_PRODUCT); \
	    $(CHGRP) -R $(INSTALL_AS_GROUP) $(INSTALLED_JAVA_PRODUCT); \
	fi
endif
ifdef CHOWN
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR)" -a -d "$(JAVA_DSTROOT)$(JAVA_INSTALL_DIR)" ]; then \
	    $(ECHO) $(CHOWN) -R $(INSTALL_AS_USER) $(INSTALLED_JAVA_PRODUCT); \
	    $(CHOWN) -R $(INSTALL_AS_USER) $(INSTALLED_JAVA_PRODUCT); \
	fi
endif
else
change-java-perms:
endif
else
change-java-perms:
endif

ifneq "" "$(INSTALLED_JAVA_PRODUCT_CLIENT)"
ifdef JAVA_DSTROOT_CLIENT
change-java-perms-client:
ifdef CHMOD
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR_CLIENT)" -a -d "$(JAVA_DSTROOT_CLIENT)$(JAVA_INSTALL_DIR_CLIENT)" ]; then \
	    $(ECHO) $(CHMOD) -R ugo-w $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	    $(CHMOD) -R ugo-w $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	    $(ECHO) $(CHMOD) -R $(INSTALL_PERMISSIONS) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	    $(CHMOD) -R $(INSTALL_PERMISSIONS) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	fi
endif
ifdef CHGRP
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR_CLIENT)" -a -d "$(JAVA_DSTROOT_CLIENT)$(JAVA_INSTALL_DIR_CLIENT)" ]; then \
	    $(ECHO) $(CHGRP) -R $(INSTALL_AS_GROUP) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	    $(CHGRP) -R $(INSTALL_AS_GROUP) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	fi
endif
ifdef CHOWN
	-$(SILENT) if [ ! -z "$(JAVA_INSTALL_DIR_CLIENT)" -a -d "$(JAVA_DSTROOT_CLIENT)$(JAVA_INSTALL_DIR_CLIENT)" ]; then \
	    $(ECHO) $(CHOWN) -R $(INSTALL_AS_USER) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	    $(CHOWN) -R $(INSTALL_AS_USER) $(INSTALLED_JAVA_PRODUCT_CLIENT); \
	fi
endif
else
change-java-perms-client:
endif
else
change-java-perms-client:
endif

compress-man-pages:
ifneq "$(strip $(MAN_PAGE_DIRECTORIES))" ""
	$(SILENT) $(ECHO) "Compressing man pages..."
	$(SILENT) $(COMPRESSMANPAGES) $(MAN_PAGE_DIRECTORIES)
endif

#
# Support for converting "bundles" (.app, .palette, .framework, 
# and .bundle directory wrappers) from the old OpenStep/Yellow/Cocoa-MacOSXServer structure
# to the new structure for MacOS X
#

# BUNDLE_STYLE controls on a per-project basis if the project wants to use
# the new structure.

# PROJTYPE_CONVERT_BUNDLE is set by the various project type specific makefiles if the 
# project type wants to use the new structure. It should be only set if the OS is MACOSX

ifeq "YES" "$(CONVERT_BUNDLE_BACKDOOR)"
PROJTYPE_CONVERT_BUNDLE = YES
endif

ifeq "YES" "$(PROJTYPE_CONVERT_BUNDLE)"
ifeq "MACOSX" "$(BUNDLE_STYLE)"
convert-bundle:
	$(MAKEFILEDIR)/convertBundle $(INSTALLED_PRODUCTS)
else
convert-bundle:
endif
else
convert-bundle:
endif

#
# rule for creating directories
#

$(DSTROOT)$(INSTALLDIR):
	$(SILENT) $(MKDIRS) $@


ifeq "$(OS)" "WINDOWS"
include $(MAKEFILEDIR)/reinstall.make
endif

