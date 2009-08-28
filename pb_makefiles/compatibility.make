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
# compatibility.make
#
# This file provides backward-compatibility with projects which used to
# be built using project_makefiles.  If you have not used any internal
# makefile api, then your old-style projects should build if this makefile
# is included.  If you have used internal api, careful examination of your
# product should indicate what changes are necessary for it to build both
# under project_makefiles and pb_makefiles.
#

#
# Variables on LHS are used by the client
#

SYM_DIR = $(SFILE_DIR)
DERIVED_SRC_DIR = $(SFILE_DIR)
PRODUCT_ROOT = $(PRODUCT_DIR)
RESOURCES_ROOT = $(GLOBAL_RESOURCE_DIR)

#
# Variables on RHS are defined by the client
#

ifeq "" "$(OTHER_YFLAGS)"
OTHER_YFLAGS = $(YFLAGS)
endif
ifeq "" "$(OTHER_LFLAGS)"
OTHER_LFLAGS = $(LFLAGS)
endif
ifeq "" "$(OTHER_PSWFLAGS)"
OTHER_PSWFLAGS = $(PSWFLAGS)
endif
ifeq "" "$(OTHER_RPCFLAGS)"
OTHER_RPCFLAGS = $(RPCFLAGS)
endif
LOCAL_LDFLAGS += $(OTHER_LIBTOOL_FLAGS)
OTHER_STRIPFLAGS += $(APP_STRIP_OPTS)
ifeq "" "$(PROFILE_BUILD_LIBS)"
PROFILE_BUILD_LIBS = $(PROF_LIBS)
endif
ifeq "" "$(V)"
V = $(VAR_NAME)
endif
ifeq "" "$(VERSION_NAME)"
VERSION_NAME = $(DEPLOY_WITH_VERSION_NAME)
endif
ifeq "" "$(VERSION_NAME)"
VERSION_NAME = UNSPECIFIED
endif
ifeq "" "$(INCLUDED_OSS)"
INCLUDED_OSS = $(INCLUDED_PLATFORMS)
INCLUDED_OSS := $(subst winnt,WINDOWS,$(INCLUDED_OSS))
INCLUDED_OSS := $(subst macos,MACOS,$(INCLUDED_OSS))
INCLUDED_OSS := $(subst nextstep,NEXTSTEP,$(INCLUDED_OSS))
INCLUDED_OSS := $(subst hpux,HPUX,$(INCLUDED_OSS))
INCLUDED_OSS := $(subst solaris,SOLARIS,$(INCLUDED_OSS))
endif
ifeq "" "$(EXCLUDED_OSS)"
EXCLUDED_OSS = $(EXCLUDED_PLATFORMS)
EXCLUDED_OSS := $(subst winnt,WINDOWS,$(EXCLUDED_OSS))
EXCLUDED_OSS := $(subst macos,MACOS,$(EXCLUDED_OSS))
EXCLUDED_OSS := $(subst nextstep,NEXTSTEP,$(EXCLUDED_OSS))
EXCLUDED_OSS := $(subst hpux,HPUX,$(EXCLUDED_OSS))
EXCLUDED_OSS := $(subst solaris,SOLARIS,$(EXCLUDED_OSS))
endif

ifeq "SOLARIS" "$(OS)"
PLATFORM_TYPE = PDO_UNIX
else
ifeq "HPUX" "$(OS)"
PLATFORM_TYPE = PDO_UNIX
else
ifeq "MACOS" "$(OS)"
PLATFORM_TYPE = NEXTSTEP
else
PLATFORM_TYPE = $(OS)
endif
endif
endif

ifeq "" "$(PUBLIC_HEADER_DIR)"
ifneq "" "$($(PLATFORM_TYPE)_PUBLIC_HEADERS_DIR)"
PUBLIC_HEADER_DIR = $($(PLATFORM_TYPE)_PUBLIC_HEADERS_DIR)
endif
endif

ifneq "" "$(PUBLIC_HEADER_DIR)"
PUBLIC_HDR_INSTALLDIR = $(PUBLIC_HEADER_DIR)
endif
ifneq "" "$(PRIVATE_HEADER_DIR)"
PRIVATE_HDR_INSTALLDIR = $(PRIVATE_HEADER_DIR)
endif
BEFORE_BUILD_RECURSION += $(OTHER_INITIAL_TARGETS)
ifeq "YES" "$(SKIP_EXPORTING_HEADERS)"
SKIP_PREBUILD = YES
endif

#
# Variables the client should provide, but doesn't
#

include $(MAKEFILEDIR)/platform.make
ALL_SUBPROJECTS := $(LIBRARIES) $(LIBRARY_SUBPROJECTS) $(FRAMEWORK_SUBPROJECTS) $(SUBPROJECTS) $(BUNDLES) $(TOOLS)  $(PALETTES) $(AGGREGATES) $(LEGACIES)
YFILES := $(filter %.y, $(OTHERLINKED))
LFILES := $(filter %.l, $(OTHERLINKED))
YMFILES := $(filter %.ym, $(OTHERLINKED))
LMFILES := $(filter %.lm, $(OTHERLINKED))
OTHERLINKED := $(filter-out %.y, $(OTHERLINKED))
OTHERLINKED := $(filter-out %.l, $(OTHERLINKED))
OTHERLINKED := $(filter-out %.ym, $(OTHERLINKED))
OTHERLINKED := $(filter-out %.lm, $(OTHERLINKED))

#
# Targets are invoked/depended on by the client
#

#
# Dependencies are provided by the client
#

.PHONY: before_install after_installhdrs after_install
echo_makefile_variable: sv
BEFORE_INSTALL += _before_install $(OTHER_INSTALL_DEPENDS)
_before_install: before_install # silly _ required because before_install was :: rule
AFTER_INSTALLHDRS += _after_installhdrs
_after_installhdrs: after_installhdrs   # similar silliness
AFTER_INSTALL += _after_install
_after_install: after_install   # similar silliness

#
# we need this hack because libtool on NT does not work without arch_only
#

ifeq "WINDOWS" "$(OS)"
ifeq "LIBRARY" "$(PROJTYPE)"
PROJTYPE_LDFLAGS += -arch_only $(ARCH)
endif
ifeq "FRAMEWORK" "$(PROJTYPE)"
PROJTYPE_LDFLAGS += -arch_only $(ARCH)
endif
endif

