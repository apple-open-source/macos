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
# versions.make
#
# Rules for projects which can be deployed using named versions.  Only
# frameworks and libraries may be versioned, and the means of versioning
# them is completely different.
#
# IMPORTED VARIABLES
#    DEPLOY_WITH_VERSION_NAME
#    DYLIB_INSTALL_NAME # ignored for frameworks
#    DYLIB_INSTALL_DIR  # ignored for frameworks
#

.PHONY: create-version-links change-current-version-link
.PHONY: create-install-version-links change-current-install-version-link

#
# Versioning is only supported on some platforms
#

ifneq "WINDOWS" "$(OS)"
ENABLE_VERSIONING = YES
endif

#
# Ignore this entire file if versioning is disabled
#

ifeq "YES" "$(ENABLE_VERSIONING)"

#
# Rules for framework projects
#

ifeq "FRAMEWORK" "$(PROJTYPE)"

BEFORE_BUILD_RECURSION += create-version-links
BEFORE_PREBUILD += create-version-links
BEFORE_INSTALLHDRS += create-install-version-links
ifneq "NO" "$(CURRENTLY_ACTIVE_VERSION)"
AFTER_BUILD += change-current-version-link
AFTER_INSTALLHDRS += change-current-install-version-link
endif

DYLIB_INSTALL_DIR := $(DYLIB_INSTALL_DIR)/Versions/$(VERSION_NAME)
NONVERSIONED_INNER_PRODUCT := $(INNER_PRODUCT)
INNER_PRODUCT := $(dir $(INNER_PRODUCT))Versions/$(VERSION_NAME)/$(notdir $(INNER_PRODUCT))

# We don't need a NONVERSIONED_INNER_PROFILED_PRODUCT for link creation,
# since it's actually created in a recursive invokation of make.
# INNER_PROFILED_PRODUCT is used only for stripping the profiled product.
INNER_PROFILED_PRODUCT := $(dir $(INNER_PROFILED_PRODUCT))Versions/$(VERSION_NAME)/$(notdir $(INNER_PROFILED_PRODUCT))

NONVERSIONED_PRIVATE_HDR_DIR := $(PRIVATE_HDR_DIR)
PRIVATE_HDR_DIR := $(dir $(PRIVATE_HDR_DIR))Versions/$(VERSION_NAME)/$(notdir $(PRIVATE_HDR_DIR))
NONVERSIONED_PUBLIC_HDR_DIR := $(PUBLIC_HDR_DIR)
PUBLIC_HDR_DIR := $(dir $(PUBLIC_HDR_DIR))Versions/$(VERSION_NAME)/$(notdir $(PUBLIC_HDR_DIR))
NONVERSIONED_PRIVATE_HDR_INSTALLDIR := $(PRIVATE_HDR_INSTALLDIR)
PRIVATE_HDR_INSTALLDIR := $(dir $(PRIVATE_HDR_INSTALLDIR))Versions/$(VERSION_NAME)/$(notdir $(PRIVATE_HDR_INSTALLDIR))
NONVERSIONED_PUBLIC_HDR_INSTALLDIR := $(PUBLIC_HDR_INSTALLDIR)
PUBLIC_HDR_INSTALLDIR := $(dir $(PUBLIC_HDR_INSTALLDIR))Versions/$(VERSION_NAME)/$(notdir $(PUBLIC_HDR_INSTALLDIR))
NONVERSIONED_GLOBAL_RESOURCE_DIR := $(GLOBAL_RESOURCE_DIR)
GLOBAL_RESOURCE_DIR := $(dir $(GLOBAL_RESOURCE_DIR))Versions/$(VERSION_NAME)/$(notdir $(GLOBAL_RESOURCE_DIR))
NONVERSIONED_WEBSERVER_RESOURCE_DIR := $(WEBSERVER_RESOURCE_DIR)
WEBSERVER_RESOURCE_DIR := $(dir $(WEBSERVER_RESOURCE_DIR))Versions/$(VERSION_NAME)/$(notdir $(WEBSERVER_RESOURCE_DIR))

create-version-links: $(dir $(NONVERSIONED_GLOBAL_RESOURCE_DIR))Versions/Current $(NONVERSIONED_INNER_PRODUCT) $(NONVERSIONED_PRIVATE_HDR_DIR) $(NONVERSIONED_PUBLIC_HDR_DIR) $(NONVERSIONED_GLOBAL_RESOURCE_DIR)

ifeq "YES" "$(BUILDING_WOFRAMEWORK)"
create-version-links: $(dir $(NONVERSIONED_WEBSERVER_RESOURCE_DIR))Versions/Current $(NONVERSIONED_WEBSERVER_RESOURCE_DIR)
endif

$(dir $(NONVERSIONED_GLOBAL_RESOURCE_DIR))Versions/Current $(dir $(NONVERSIONED_WEBSERVER_RESOURCE_DIR))Versions/Current:
	$(RM) -rf $@
	$(MKDIRS) $(dir $@)
	$(SYMLINK) $(VERSION_NAME) $@

$(NONVERSIONED_INNER_PRODUCT) $(NONVERSIONED_PRIVATE_HDR_DIR) $(NONVERSIONED_PUBLIC_HDR_DIR) $(NONVERSIONED_GLOBAL_RESOURCE_DIR) $(NONVERSIONED_WEBSERVER_RESOURCE_DIR) $(DSTROOT)$(NONVERSIONED_PRIVATE_HDR_INSTALLDIR) $(DSTROOT)$(NONVERSIONED_PUBLIC_HDR_INSTALLDIR):
	$(RM) -rf $@
	$(MKDIRS) $(dir $@)
	$(SYMLINK) Versions/Current/$(notdir $@) $@

change-current-version-link:
	$(RM) -rf $(dir $(NONVERSIONED_GLOBAL_RESOURCE_DIR))Versions/Current
	$(MKDIRS) $(dir $(NONVERSIONED_GLOBAL_RESOURCE_DIR))Versions
	$(SYMLINK) $(VERSION_NAME) $(dir $(NONVERSIONED_GLOBAL_RESOURCE_DIR))Versions/Current

create-install-version-links: $(DSTROOT)$(NONVERSIONED_PRIVATE_HDR_INSTALLDIR)\
  $(DSTROOT)$(NONVERSIONED_PUBLIC_HDR_INSTALLDIR)
	
change-current-install-version-link:
	$(RM) -rf $(DSTROOT)$(dir $(NONVERSIONED_PUBLIC_HDR_INSTALLDIR))Versions/Current
	$(MKDIRS) $(DSTROOT)$(dir $(NONVERSIONED_PUBLIC_HDR_INSTALLDIR))Versions
	$(SYMLINK) $(VERSION_NAME) $(DSTROOT)$(dir $(NONVERSIONED_PUBLIC_HDR_INSTALLDIR))Versions/Current

endif	# PROJTYPE == FRAMEWORK

ifeq "LIBRARY" "$(PROJTYPE)"
ifneq "STATIC" "$(LIBRARY_STYLE)"

#
# Rules for library projects
#

.PHONY: change-current-version-link

PRODUCTS += $(NONVERSIONED_PRODUCT)
BEFORE_PREBUILD += create-version-links
ifneq "NO" "$(CURRENTLY_ACTIVE_VERSION)"
AFTER_BUILD += change-current-version-link
endif

DYLIB_INSTALL_NAME := $(subst $(NAME),$(NAME).$(VERSION_NAME),$(DYLIB_INSTALL_NAME))
NONVERSIONED_PRODUCT := $(PRODUCT)
PRODUCT := $(dir $(PRODUCT))$(subst $(NAME),$(NAME).$(VERSION_NAME),$(notdir $(PRODUCT)))

$(NONVERSIONED_PRODUCT) change-current-version-link:
	$(RM) -rf $(NONVERSIONED_PRODUCT)
	$(SYMLINK) $(notdir $(PRODUCT)) $(NONVERSIONED_PRODUCT)

endif
endif

endif
