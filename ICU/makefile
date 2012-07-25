##
# Wrapper makefile for ICU
# Copyright (C) 2003-2012 Apple Inc. All rights reserved.
#
# See http://www.gnu.org/manual/make/html_chapter/make_toc.html#SEC_Contents
# for documentation on makefiles. Most of this was culled from the ncurses makefile.
#
##

#################################
#################################
# MAKE VARS
#################################
#################################

# ':=' denotes a "simply expanded" variable. It's value is
# set at the time of definition and it never recursively expands
# when used. This is in contrast to using '=' which denotes a
# recursively expanded variable.

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
WINDOWS=NO
LINUX=NO
ARCH64=NO

SRCROOT=$(shell pwd)
OBJROOT=$(SRCROOT)/build
DSTROOT=$(OBJROOT)
SYMROOT=$(OBJROOT)
CROSSHOST_OBJROOT=$(OBJROOT)/crossbuildhost
APPLE_INTERNAL_DIR=/AppleInternal
RC_ARCHS=
MAC_OS_X_VERSION_MIN_REQUIRED=1060
OSX_HOST_VERSION_MIN_STRING=10.7
IOS_VERSION_TARGET_STRING=6.0
OSX_VERSION_TARGET_STRING=10.8

UNAME_PROCESSOR=$(shell uname -p)
ifneq "$(RC_ARCHS)" ""
	ifneq "$(SDKROOT)" ""
		CROSS_BUILD=YES
	else ifeq "$(RC_INDIGO)" "YES"
		CROSS_BUILD=YES
	else
		CROSS_BUILD=NO
	endif
	INSTALLHDRS_ARCH=-arch $(shell echo $(RC_ARCHS) | cut -d' ' -f1)
else
	CROSS_BUILD=NO
	INSTALLHDRS_ARCH=
endif
$(info # RC_ARCHS=$(RC_ARCHS))
$(info # INSTALLHDRS_ARCH=$(INSTALLHDRS_ARCH))
$(info # buildhost=$(UNAME_PROCESSOR))

# FORCEENDIAN below is to override silly configure behavior in which if
# __APPLE_CC__ is defined and archs are in { ppc, ppc64, i386, x86_64 }
# then it assumes a universal build (ac_cv_c_bigendian=universal) with
# data file initially built big-endian.
#
ifeq "$(CROSS_BUILD)" "YES"
	RC_ARCHS_FIRST=$(shell echo $(RC_ARCHS) | cut -d' ' -f1)
	TARGET_SPEC=$(RC_ARCHS_FIRST)-apple-darwin10.0.0
	ENV_CONFIGURE_ARCHS=$(RC_ARCHS:%=-arch %)
	ICUPKGTOOLIBS="$(CROSSHOST_OBJROOT)/lib:$(CROSSHOST_OBJROOT)/stubdata"
	ICUPKGTOOL=$(CROSSHOST_OBJROOT)/bin/icupkg
	ifeq "$(filter-out i386 x86_64,$(RC_ARCHS))" ""
		FORCEENDIAN= ac_cv_c_bigendian=no
	else
		FORCEENDIAN=
	endif
else ifeq "$(LINUX)" "YES"
	TARGET_SPEC=$(UNAME_PROCESSOR)-unknown-linux-gnu
	ENV_CONFIGURE_ARCHS=
	ICUPKGTOOLIBS="$(OBJROOT)/lib:$(OBJROOT)/stubdata"
	ICUPKGTOOL=$(OBJROOT)/bin/icupkg
	FORCEENDIAN=
else
	TARGET_SPEC=$(UNAME_PROCESSOR)-apple-darwin10.0.0
	ENV_CONFIGURE_ARCHS=
	ICUPKGTOOLIBS="$(OBJROOT)/lib:$(OBJROOT)/stubdata"
	ICUPKGTOOL=$(OBJROOT)/bin/icupkg
	FORCEENDIAN=
endif
$(info # TARGET_SPEC=$(TARGET_SPEC))
$(info # ENV_CONFIGURE_ARCHS=$(ENV_CONFIGURE_ARCHS))

ifeq "$(RC_INDIGO)" "YES"
	-include $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.indigo
	ifndef SDKROOT
		SDKROOT=$(INDIGO_PREFIX)
	endif
	DEST_ROOT=$(DSTROOT)/$(INDIGO_PREFIX)/
else
	DEST_ROOT=$(DSTROOT)
endif
$(info # SDKROOT=$(SDKROOT))
$(info # RC_INDIGO=$(RC_INDIGO))
$(info # CROSS_BUILD=$(CROSS_BUILD))
$(info # DSTROOT=$(DSTROOT))
$(info # DEST_ROOT=$(DEST_ROOT))

ICU_TARGET_VERSION_FOR_TZ_EXTRA :=
ifeq "$(filter arm armv6 armv7,$(RC_ARCHS))" ""
	THUMB_FLAG =
	ifneq "$(SDKROOT)" ""
		ICU_TARGET_VERSION_FOR_TZ_EXTRA := -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING)
	endif
else
	THUMB_FLAG = -mthumb
endif

# even for a crossbuild host build, we want to use the target's tzdata
export TZDATA:=$(lastword $(wildcard $(SDKROOT)/usr/local/share/tz/tzdata*.tar.gz))
$(info # TZDATA=$(TZDATA))

ifeq "$(WINDOWS)" "YES"
	EMBEDDED:=0
else ifeq "$(LINUX)" "YES"
	CC := gcc
	CXX := g++
	EMBEDDED:=0
	ISYSROOT =
else ifeq "$(SDKROOT)" ""
	CC := $(shell xcrun -find cc)
	CXX := $(shell xcrun -find c++)
	NM := $(shell xcrun -find nm)
	STRIPCMD := $(shell xcrun -find strip)
	EMBEDDED:=$(shell $(CXX) -E -dM -x c -include TargetConditionals.h /dev/null | fgrep TARGET_OS_EMBEDDED | cut -d' ' -f3)
	ISYSROOT =
else
	CC := $(shell xcrun -sdk $(SDKROOT) -find cc)
	CXX := $(shell xcrun -sdk $(SDKROOT) -find c++)
	NM := $(shell xcrun -sdk $(SDKROOT) -find nm)
	STRIPCMD := $(shell xcrun -sdk $(SDKROOT) -find strip)
	EMBEDDED:=$(shell $(CXX) -E -dM -x c -isysroot $(SDKROOT) -include TargetConditionals.h /dev/null | fgrep TARGET_OS_EMBEDDED | cut -d' ' -f3)
	ISYSROOT:= -isysroot $(SDKROOT)
endif
$(info # CC=$(CC))
$(info # CXX=$(CXX))

ifeq "$(EMBEDDED)" "1"
	export APPLE_EMBEDDED=YES
	DISABLE_DRAFT=YES
else ifeq "$(RC_INDIGO)" "YES"
	export APPLE_EMBEDDED=YES
	DISABLE_DRAFT=YES
else
	export APPLE_EMBEDDED=NO
	DISABLE_DRAFT=NO
endif

ifeq "$(APPLE_EMBEDDED)" "YES"
	ICU_TARGET_VERSION := -miphoneos-version-min=$(IOS_VERSION_TARGET_STRING)
else
	ICU_TARGET_VERSION := 
endif
$(info # ICU_TARGET_VERSION=$(ICU_TARGET_VERSION))

ifeq "$(DISABLE_DRAFT)" "YES"
	DRAFT_FLAG=--disable-draft
else
	DRAFT_FLAG=
endif

ifndef RC_ProjectSourceVersion
ifdef RC_PROJECTSOURCEVERSION
	RC_ProjectSourceVersion=$(RC_PROJECTSOURCEVERSION)
endif
endif

# An Apple submission version (passed in RC_ProjectSourceVersion for official builds) is
# X[.Y[.Z]]
# where X is in range 0-214747, Y and Z are in range 0-99 (with no leading zeros).
# The value for the SourceVersion property in version.plists will be calculated as
# (X*10000 + Y*100 + Z). So we want U_ICU_VERSION_BUILDLEVEL_NUM to be Y*100 + Z
ifneq "$(RC_ProjectSourceVersion)" ""
	ifeq "$(WINDOWS)" "YES"
		ICU_BUILD_Y  := $(shell echo $(RC_ProjectSourceVersion) | sed -r -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\3/')
		ICU_BUILD_Z1 := $(shell echo $(RC_ProjectSourceVersion) | sed -r -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\5/')
		ICU_BUILD_Z2 := $(shell echo $(RC_ProjectSourceVersion) | sed -r -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\6/')
	else
		ICU_BUILD_Y  := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\3/')
		ICU_BUILD_Z1 := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\5/')
		ICU_BUILD_Z2 := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)(\.([0-9]{1,2})(\.([0-9])([0-9])?)?)?/\6/')
	endif
	ifeq "$(ICU_BUILD_Y)" ""
		ICU_BUILD := 0
	else
		ICU_BUILD := $(subst a,$(ICU_BUILD_Y),abc)
		ifeq "$(ICU_BUILD_Z1)" ""
			ICU_BUILD := $(subst b,0,$(ICU_BUILD))
			ICU_BUILD := $(subst c,0,$(ICU_BUILD))
		else
			ifeq "$(ICU_BUILD_Z2)" ""
				ICU_BUILD := $(subst b,0,$(ICU_BUILD))
				ICU_BUILD := $(subst c,$(ICU_BUILD_Z1),$(ICU_BUILD))
			else
				ICU_BUILD := $(subst b,$(ICU_BUILD_Z1),$(ICU_BUILD))
				ICU_BUILD := $(subst c,$(ICU_BUILD_Z2),$(ICU_BUILD))
			endif
		endif
	endif
else
	ICU_BUILD := 0
endif
$(info # ICU_BUILD=$(ICU_BUILD))

# Disallow $(SRCROOT) == $(OBJROOT)
ifeq ($(OBJROOT), $(SRCROOT))
$(error SRCROOT same as OBJROOT)
endif

# Disallow cross builds on Windows/Linux for now
# (since those builds are not out-of-source as required for cross-builds)
ifeq "$(CROSS_BUILD)" "YES"
ifeq "$(WINDOWS)" "YES"
$(error Cross-builds currently not allowed on Windows)
endif
ifeq "$(LINUX)" "YES"
$(error Cross-builds currently not allowed on Linux)
endif
endif

#################################
# Headers
#################################

# For installhdrs. Not every compiled module has an associated header. Normally,
# ICU installs headers as a sub-targe of the install target. But since we only want
# certain libraries to install (and since we link all of our own .o modules), we need
# invoke the headers targets ourselves. This may be problematic because there isn't a
# good way to dist-clean afterwards...we need to do explicit dist-cleans, especially if
# install the extra libraries.

EXTRA_HDRS =
# EXTRA_HDRS = ./extra/ustdio/ ./layout/ 
ifeq "$(APPLE_EMBEDDED)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else ifeq "$(WINDOWS)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ ./io/ $(EXTRA_HDRS)
endif
ifeq "$(WINDOWS)" "YES"
	PRIVATE_HDR_PREFIX=$(APPLE_INTERNAL_DIR)
else ifeq "$(APPLE_EMBEDDED)" "YES"
	HDR_PREFIX=/usr
	PRIVATE_HDR_PREFIX=/usr/local
else
	PRIVATE_HDR_PREFIX=/usr/local
endif

#################################
# Install
#################################

# For install. We currently don't install EXTRA_LIBS. We also don't install the data 
# directly into the ICU library. It is now installed at /usr/share/icu/*.dat. Thus we 
# don't use DATA_OBJ anymore. This could change if we decide to move the data back into
# the icucore monolithic library.

INSTALL = /usr/bin/install
COMMON_OBJ = ./common/*.o
I18N_OBJ = ./i18n/*.o
IO_OBJ = ./io/*.o
STUB_DATA_OBJ = ./stubdata/*.o
EXTRA_LIBS =
#EXTRA_LIBS =./extra/ ./layout/ ./tools/ctestfw/ ./tools/toolutil/
#DATA_OBJ = ./data/out/build/*.o
ifeq "$(APPLE_EMBEDDED)" "YES"
    DYLIB_OBJS=$(COMMON_OBJ) $(I18N_OBJ) $(STUB_DATA_OBJ)
else ifeq "$(WINDOWS)" "YES"
    DYLIB_OBJS=$(COMMON_OBJ) ./common/common.res $(I18N_OBJ) $(STUB_DATA_OBJ)
else
    DYLIB_OBJS=$(COMMON_OBJ) $(I18N_OBJ) $(IO_OBJ) $(STUB_DATA_OBJ)
endif

#################################
# Sources
#################################

# For installsrc (B&I)
# Note that installsrc is run on the system from which ICU is submitted, which
# may be a different environment than the one for a which a build is targeted.

INSTALLSRC_VARFILES=./ICU_embedded.order ./minimalapis.txt ./minimalapisTest.c ./minimalpatchconfig.txt ./windowspatchconfig.txt ./patchconfig.txt ./crosshostpatchconfig.txt

#################################
# Cleaning
#################################

#We need to clean after installing.

EXTRA_CLEAN = 

# Some directories aren't cleaned recursively. Clean them manually...
MANUAL_CLEAN_TOOLS = ./tools/dumpce
MANUAL_CLEAN_EXTRA = ./extra/scrptrun ./samples/layout ./extra/ustdio ./extra
MANUAL_CLEAN_TEST = ./test/collperf ./test/iotest ./test/letest ./test/thaitest ./test/threadtest ./test/testmap ./test 
MANUAL_CLEAN_SAMPLE = ./samples/layout ./samples

CLEAN_SUBDIR = ./stubdata ./common ./i18n ./io ./layout ./layoutex ./data ./tools ./$(MANUAL_CLEAN_TOOLS) $(MANUAL_CLEAN_EXTRA) $(MANUAL_CLEAN_TEST) $(MANUAL_CLEAN_SAMPLE) 

#################################
# Config flags
#################################

ifeq "$(WINDOWS)" "YES"
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples --disable-icuio \
		--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG)
else ifeq "$(LINUX)" "YES"
	ifeq "$(ARCH64)" "YES"
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
			--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=64 \
			$(DRAFT_FLAG)
	else
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
			--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=32 \
			$(DRAFT_FLAG)
	endif
else ifeq "$(APPLE_EMBEDDED)" "YES"
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples --disable-icuio \
		--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG)
else
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
		--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG)
endif

#################################
# Install paths
#################################

# This may or may not be an appropriate name for the icu dylib. This naming scheme is 
# an attempt to follow the icu convention in naming the dylib and then having symbolic 
# links of easier to remember library names point it it. *UPDATE* the version and 
# sub-version variables as needed. The Core version should be 'A' until the core
# version changes it's API...that is a new version isn't backwards compatible.
# The ICU version/subversion should reflect the actual ICU version.

LIB_NAME = icucore
ICU_VERS = 49
ICU_SUBVERS = 1
CORE_VERS = A

ifeq "$(WINDOWS)" "YES"
	DYLIB_SUFF = dll
	libdir = /AppleInternal/bin/
	winlibdir = /AppleInternal/lib/
else ifeq "$(LINUX)" "YES"
	DYLIB_SUFF = so
	ifeq "$(ARCH64)" "YES"
		libdir = /usr/lib64/
	else
		libdir = /usr/lib/
	endif
	winlibdir =
else
	DYLIB_SUFF = dylib
	libdir = /usr/lib/
	winlibdir =
endif

DYLIB = lib$(LIB_NAME).$(DYLIB_SUFF)
DYLIB_DEBUG = lib$(LIB_NAME)_debug.$(DYLIB_SUFF)
DYLIB_PROFILE = lib$(LIB_NAME)_profile.$(DYLIB_SUFF)
ifeq "$(WINDOWS)" "YES"
	INSTALLED_DYLIB = $(LIB_NAME).$(DYLIB_SUFF)
	INSTALLED_DYLIB_DEBUG = $(LIB_NAME)_debug.$(DYLIB_SUFF)
	INSTALLED_DYLIB_PROFILE = $(LIB_NAME)_profile.$(DYLIB_SUFF)
else ifeq "$(LINUX)" "YES"
	INSTALLED_DYLIB = lib$(LIB_NAME).$(DYLIB_SUFF)
	INSTALLED_DYLIB_DEBUG = lib$(LIB_NAME)_debug.$(DYLIB_SUFF)
	INSTALLED_DYLIB_PROFILE = lib$(LIB_NAME)_profile.$(DYLIB_SUFF)
else
	INSTALLED_DYLIB = lib$(LIB_NAME).$(CORE_VERS).$(DYLIB_SUFF)
	INSTALLED_DYLIB_DEBUG = lib$(LIB_NAME).$(CORE_VERS)_debug.$(DYLIB_SUFF)
	INSTALLED_DYLIB_PROFILE = lib$(LIB_NAME).$(CORE_VERS)_profile.$(DYLIB_SUFF)
endif

INSTALLED_DYLIB_icu = INSTALLED_DYLIB
INSTALLED_DYLIB_debug = INSTALLED_DYLIB_DEBUG
INSTALLED_DYLIB_profile = INSTALLED_DYLIB_PROFILE
DYLIB_icu = DYLIB
DYLIB_debug = DYLIB_DEBUG
DYLIB_profile = DYLIB_PROFILE

#################################
# Data files
#################################

datadir=/usr/share/icu/
OPEN_SOURCE_VERSIONS_DIR=/usr/local/OpenSourceVersions/
OPEN_SOURCE_LICENSES_DIR=/usr/local/OpenSourceLicenses/
ICU_DATA_DIR= data/out
B_DATA_FILE=icudt$(ICU_VERS)b.dat
L_DATA_FILE=icudt$(ICU_VERS)l.dat

#################################
# Info tool
#################################

localtooldir=/usr/local/bin/

INFOTOOL = icuinfo

INFOTOOL_OBJS = ./tools/icuinfo/icuinfo.o ./tools/toolutil/udbgutil.o ./tools/toolutil/uoptions.o

#################################
# Environment variables
#################################

# $(RC_ARCHS:%=-arch %) is a substitution reference. It denotes, in this case,
# for every value <val> in RC_ARCHS, replace it with "-arch <val>". Substitution
# references have the form $(var:a=b). We can insert the strip and prebinding commands
# into CFLAGS (and CXXFLAGS). This controls a lot of the external variables so we don't
# need to directly modify the ICU files (like for CFLAGS, etc).

LIBOVERRIDES=LIBICUDT="-L$(OBJROOT) -l$(LIB_NAME)" \
	LIBICUUC="-L$(OBJROOT) -l$(LIB_NAME)" \
	LIBICUI18N="-L$(OBJROOT) -l$(LIB_NAME)"

# For normal Windows builds set the ENV= options here; for debug Windows builds set the ENV_DEBUG=
# options here and also the update the LINK.EXE lines in the TARGETS section below.
ifeq "$(WINDOWS)" "YES"
	export TZ_EXTRA_CXXFLAGS:=
	ifeq "$(ICU_BUILD)" "0"
		CPPOPTIONS = CPPFLAGS="-DU_DISABLE_RENAMING=1"
	else
		CPPOPTIONS = CPPFLAGS="-DU_DISABLE_RENAMING=1 -DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)"
	endif
	ENV= CFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t" LDFLAGS="/NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF"
	ENV_CONFIGURE= $(CPPOPTIONS) CFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t"
	ENV_DEBUG= CFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" CXXFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc" LDFLAGS="/DEBUG /DYNAMICBASE"
	ENV_PROFILE=
else ifeq "$(LINUX)" "YES"
	export TZ_EXTRA_CXXFLAGS:=
	ifeq "$(ICU_BUILD)" "0"
		CPPOPTIONS = CPPFLAGS="-DU_DISABLE_RENAMING=1"
	else
		CPPOPTIONS = CPPFLAGS="-DU_DISABLE_RENAMING=1 -DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)"
	endif
	ifeq "$(ARCH64)" "YES"
		ENV_CONFIGURE=	$(CPPOPTIONS) APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib64"
		
		ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib64"
			
		ENV_DEBUG = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib64"
		
		ENV_PROFILE = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib64"
	else
		ENV_CONFIGURE=	$(CPPOPTIONS) APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
		
		ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
			
		ENV_DEBUG = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
		
		ENV_PROFILE = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	endif
else
	export TZ_EXTRA_CXXFLAGS:=-DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) $(ICU_TARGET_VERSION_FOR_TZ_EXTRA)
	ifeq "$(ICU_BUILD)" "0"
		CPPOPTIONS = CPPFLAGS="-DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED)"
	else
		CPPOPTIONS = CPPFLAGS="-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED)"
	endif
	ENV_CONFIGURE=	$(CPPOPTIONS) APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" $(FORCEENDIAN)\
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	
	ENV=	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
		
	ENV_DEBUG = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	
	ENV_PROFILE = APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"

	ENV_CONFIGURE_BUILDHOST =	$(CPPOPTIONS) APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"
	
	ENV_BUILDHOST =	APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"/usr/share/icu/\\\"\" -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
		TZ_EXTRA_CXXFLAGS="$(TZ_EXTRA_CXXFLAGS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)/usr/local/lib"

endif
	
ENV_icu = ENV
ENV_debug = ENV_DEBUG
ENV_profile = ENV_PROFILE

ifeq "$(APPLE_EMBEDDED)" "YES"
	ORDERFILE=$(SRCROOT)/ICU_embedded.order
else
	ORDERFILE=/usr/local/lib/OrderFiles/libicucore.order
endif
ifeq "$(shell test -f $(ORDERFILE) && echo YES )" "YES"
       SECTORDER_FLAGS=-sectorder __TEXT __text $(ORDERFILE)
else
       SECTORDER_FLAGS=
endif


#################################
#################################
# TARGETS
#################################
#################################

.PHONY : icu check installsrc installhdrs installhdrsint clean install debug debug-install crossbuildhost
.DELETE_ON_ERROR :

icu debug profile : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
		$(MAKE) $($(ENV_$@)); \
		if test "$(WINDOWS)" = "YES"; then \
			(cd common; \
				rc.exe /folibicuuc.res $(CPPFLAGS) -DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../i18n \
					"-DDEFAULT_ICU_PLUGINS=\"/AppleInternal/lib/icu\" " -DU_LOCAL_SERVICE_HOOK=1 libicuuc.rc; \
			); \
			(cd i18n; \
				rc.exe /folibicuin.res $(CPPFLAGS) -DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../common libicuin.rc; \
			); \
			if test "$@" = "debug"; then \
				(cd common; \
					LINK.EXE /subsystem:console /DLL /nologo /base:"0x4a800000" /DYNAMICBASE /DEBUG \
						/IMPLIB:../lib/libicuuc_$@.lib /out:../lib/libicuuc_$@.dll \
						*.o libicuuc.res ../stubdata/icudt.lib advapi32.lib; \
				); \
				(cd i18n; \
					LINK.EXE /subsystem:console /DLL /nologo /base:"0x4a900000" /DYNAMICBASE /DEBUG \
						/IMPLIB:../lib/libicuin_$@.lib /out:../lib/libicuin_$@.dll \
						*.o libicuin.res ../lib/libicuuc_$@.lib ../stubdata/icudt.lib advapi32.lib; \
				); \
			else \
				mkdir -p lib/shim; \
				(cd common; \
					rm -f icuuc40shim.o; \
					rm -f ../lib/libicuuc.dll.manifest; \
					rm -f ../lib/icuuc40.dll.manifest; \
					LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
						/IMPLIB:../lib/libicuuc.lib /out:../lib/libicuuc.dll \
						*.o libicuuc.res ../stubdata/icudt.lib advapi32.lib; \
					mt.exe -nologo -manifest ../lib/libicuuc.dll.manifest -outputresource:"../lib/libicuuc.dll;2"; \
					cl -DU_DISABLE_RENAMING=1 -DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD) \
						-DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../i18n \
						-DU_LOCAL_SERVICE_HOOK=1 -DWIN32 -DU_COMMON_IMPLEMENTATION \
						/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t \
						/c /Foicuuc40shim.o icuuc40shim.cpp; \
					rc.exe /foicuuc40shim.res $(CPPFLAGS) -DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../i18n \
						"-DDEFAULT_ICU_PLUGINS=\"/AppleInternal/lib/icu\" " -DU_LOCAL_SERVICE_HOOK=1 icuuc40shim.rc; \
					LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
						/IMPLIB:../lib/shim/icuuc.lib /out:../lib/icuuc40.dll \
						icuuc40shim.o icuuc40shim.res ../lib/libicuuc.lib ../stubdata/icudt.lib advapi32.lib; \
					mt.exe -nologo -manifest ../lib/icuuc40.dll.manifest -outputresource:"../lib/icuuc40.dll;2"; \
				); \
				(cd i18n; \
					rm -f icuin40shim.o; \
					rm -f ../lib/libicuin.dll.manifest; \
					rm -f ../lib/icuin40.dll.manifest; \
					LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
						/IMPLIB:../lib/libicuin.lib /out:../lib/libicuin.dll \
						*.o libicuin.res ../lib/libicuuc.lib ../stubdata/icudt.lib advapi32.lib; \
					mt.exe -nologo -manifest ../lib/libicuin.dll.manifest -outputresource:"../lib/libicuin.dll;2"; \
					cl -DU_DISABLE_RENAMING=1 -DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD) \
						-DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../common \
						-DU_LOCAL_SERVICE_HOOK=1 -DWIN32 -DU_I18N_IMPLEMENTATION \
						/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t \
						/c /Foicuin40shim.o icuin40shim.cpp; \
					rc.exe /foicuin40shim.res $(CPPFLAGS) -DU_RELEASE=1 -D_CRT_SECURE_NO_DEPRECATE -I. -I../common \
						"-DDEFAULT_ICU_PLUGINS=\"/AppleInternal/lib/icu\" " -DU_LOCAL_SERVICE_HOOK=1 icuin40shim.rc; \
					LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
						/IMPLIB:../lib/shim/icuin.lib /out:../lib/icuin40.dll \
						icuin40shim.o icuin40shim.res ../lib/libicuin.lib ../stubdata/icudt.lib advapi32.lib; \
					mt.exe -nologo -manifest ../lib/icuin40.dll.manifest -outputresource:"../lib/icuin40.dll;2"; \
				); \
			fi; \
		else \
			if test "$(LINUX)" = "YES"; then \
				if test "$(ARCH64)" = "YES"; then \
					$($(ENV_$@)) $(CXX) \
						-m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden \
						$(CXXFLAGS) $(LDFLAGS) -shared -Wl,-Bsymbolic -Wl,-soname,$($(INSTALLED_DYLIB_$@)) -Wl,-L/usr/lib64/ -ldl \
						-o ./$($(INSTALLED_DYLIB_$@)) $(DYLIB_OBJS); \
				else \
					$($(ENV_$@)) $(CXX) \
						-m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden \
						$(CXXFLAGS) $(LDFLAGS) -shared -Wl,-Bsymbolic -Wl,-soname,$($(INSTALLED_DYLIB_$@)) -ldl \
						-o ./$($(INSTALLED_DYLIB_$@)) $(DYLIB_OBJS); \
				fi; \
			else \
				tmpfile=`mktemp -t weakexternal.XXXXXX` || exit 1; \
				$(NM) -m $(RC_ARCHS:%=-arch %) $(DYLIB_OBJS) | fgrep "weak external" | fgrep -v "undefined" | sed -e 's/.*weak external[^_]*//' | sort | uniq | cat >$$tmpfile; \
				$($(ENV_$@)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
					$(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG) \
					$(CXXFLAGS) $(LDFLAGS) -single_module $(SECTORDER_FLAGS) -unexported_symbols_list $$tmpfile -dead_strip \
					-install_name $(libdir)$($(INSTALLED_DYLIB_$@)) -o ./$($(INSTALLED_DYLIB_$@)) $(DYLIB_OBJS); \
				if test "$@" = "icu"; then \
					ln -fs  $(INSTALLED_DYLIB) $(DYLIB); \
					$($(ENV_$@)) $(CXX) $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG) \
						$(LDFLAGS) -Wl,-S -Wl,-x -dead_strip -o ./$(INFOTOOL) $(INFOTOOL_OBJS) -L./ -l$(LIB_NAME) ; \
				fi; \
			fi; \
			if test -f ./$(ICU_DATA_DIR)/$(B_DATA_FILE); then \
				ln -fs ./$(ICU_DATA_DIR)/$(B_DATA_FILE); \
			fi; \
			if test -f ./$(ICU_DATA_DIR)/$(L_DATA_FILE); then \
				ln -fs ./$(ICU_DATA_DIR)/$(L_DATA_FILE); \
			else \
				DYLD_LIBRARY_PATH=$(ICUPKGTOOLIBS) \
				$(ICUPKGTOOL) -tl ./$(ICU_DATA_DIR)/$(B_DATA_FILE) $(L_DATA_FILE); \
			fi; \
		fi; \
	);

crossbuildhost : $(CROSSHOST_OBJROOT)/Makefile
	(cd $(CROSSHOST_OBJROOT); \
		$(MAKE) $($(ENV_BUILDHOST)); \
	);

check : icu
ifneq "$(CROSS_BUILD)" "YES"
	(cd $(OBJROOT); \
		ICU_DATA=$(OBJROOT) $(MAKE) $(ENV) check; \
	);
else
	$(warning check not supported for cross-build)
endif

check-debug: debug
ifneq "$(CROSS_BUILD)" "YES"
	(cd $(OBJROOT); \
		ICU_DATA=$(OBJROOT) $(MAKE) $(ENV_DEBUG) check; \
	);
else
	$(warning check not supported for cross-build)
endif

samples: icu
	(cd $(OBJROOT)/samples; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

extra: icu
	(cd $(OBJROOT)/extra; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

ifneq "$(CROSS_BUILD)" "YES"
$(OBJROOT)/Makefile : 
else
$(OBJROOT)/Makefile : crossbuildhost
endif
	if test ! -d $(OBJROOT); then \
		mkdir -p $(OBJROOT); \
	fi;
	cp -Rpf $(SRCROOT)/icuSources/* $(OBJROOT);
	if test "$(WINDOWS)" = "YES"; then \
		(cd $(OBJROOT)/data/unidata; mv base_unidata/*.txt .;); \
		(cd $(OBJROOT)/data/unidata/norm2; mv base_norm2/*.txt .;); \
		(cd $(OBJROOT)/data/in; mv base_in/*.nrm .; mv base_in/*.icu .;); \
		(cd $(OBJROOT); $(ENV_CONFIGURE) ./runConfigureICU Cygwin/MSVC $(CONFIG_FLAGS);) \
	elif test "$(LINUX)" = "YES"; then \
		(cd $(OBJROOT)/data/unidata; mv base_unidata/*.txt .;); \
		(cd $(OBJROOT)/data/unidata/norm2; mv base_norm2/*.txt .;); \
		(cd $(OBJROOT)/data/in; mv base_in/*.nrm .; mv base_in/*.icu .;); \
		(cd $(OBJROOT); $(ENV_CONFIGURE) ./runConfigureICU Linux $(CONFIG_FLAGS);) \
	elif test "$(CROSS_BUILD)" = "YES"; then \
		(cd $(OBJROOT); $(ENV_CONFIGURE) ./configure --host=$(TARGET_SPEC) --with-cross-build=$(CROSSHOST_OBJROOT) $(CONFIG_FLAGS);) \
	else \
		(cd $(OBJROOT); $(ENV_CONFIGURE) ./runConfigureICU MacOSX $(CONFIG_FLAGS);) \
	fi;
	if test "$(APPLE_EMBEDDED)" = "YES"; then \
		(cd $(OBJROOT)/common/unicode/; patch <$(SRCROOT)/minimalpatchconfig.txt;) \
	elif test "$(WINDOWS)" = "YES"; then \
		(cd $(OBJROOT)/common/unicode/; patch <$(SRCROOT)/windowspatchconfig.txt;) \
	else \
		(cd $(OBJROOT)/common/unicode/; patch <$(SRCROOT)/patchconfig.txt;) \
	fi; \
	# used to copy the Makefile.local from common & i18n in $(SRCROOT) to $(OBJROOT), no longer needed since we copy all of $(SRCROOT)

# for the tools that build the data file, cannot set UDATA_DEFAULT_ACCESS = UDATA_ONLY_PACKAGES
# as minimalpatchconfig.txt does; need different patches for the host build.
$(CROSSHOST_OBJROOT)/Makefile : 
	if test ! -d $(CROSSHOST_OBJROOT); then \
		mkdir -p $(CROSSHOST_OBJROOT); \
	fi;
	cp -Rpf $(SRCROOT)/icuSources/* $(CROSSHOST_OBJROOT);
	(cd $(CROSSHOST_OBJROOT); $(ENV_CONFIGURE_BUILDHOST) ./runConfigureICU MacOSX $(CONFIG_FLAGS);)
	(cd $(CROSSHOST_OBJROOT)/common/unicode/; patch <$(SRCROOT)/crosshostpatchconfig.txt;)
	# used to copy the Makefile.local from common & i18n in $(SRCROOT) to $(CROSSHOST_OBJROOT), no longer needed since we copy all of $(SRCROOT)

#################################
# B&I TARGETS
#################################

# Since our sources are in icuSources (ignore the ICU subdirectory for now), we wish to 
# copy them to somewhere else. We tar it to stdout, cd to the appropriate directory, and
# untar from stdin.  We then look for all the CVS directories and remove them. We may have 
# to remove the .cvsignore files also.

installsrc :
	if test ! -d $(SRCROOT); then mkdir $(SRCROOT); fi;
	if test -d $(SRCROOT)/icuSources ; then rm -rf $(SRCROOT)/icuSources; fi;
	tar cf - ./makefile ./ICU.plist ./license.html ./icuSources $(INSTALLSRC_VARFILES) | (cd $(SRCROOT) ; tar xfp -); \
	for i in `find $(SRCROOT)/icuSources/ | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done
	for j in `find $(SRCROOT)/icuSources/ | grep ".cvsignore"` ; do \
		if test -f $$j ; then \
			rm -f $$j; \
		fi; \
	done

# This works. Just not for ~ in the DEST_ROOT. We run configure first (in case it hasn't 
# been already). Then we make the install-headers target on specific makefiles (since 
# not every subdirectory/sub-component has a install-headers target).

# installhdrs should be no-op for RC_INDIGO
ifeq "$(RC_INDIGO)" "YES"
installhdrs :
else
installhdrs : installhdrsint
endif
	
installhdrsint : $(OBJROOT)/Makefile
	(cd $(OBJROOT); \
		for subdir in $(HDR_MAKE_SUBDIR); do \
			(cd $$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-headers); \
		done; \
		if test "$(APPLE_EMBEDDED)" = "YES"; then \
			if test ! -d $(DEST_ROOT)$(HDR_PREFIX)/include/unicode/; then \
				$(INSTALL) -d -m 0755 $(DEST_ROOT)$(HDR_PREFIX)/include/unicode/; \
			fi; \
			if test -d $(DEST_ROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/; then \
				(cd $(DEST_ROOT)$(PRIVATE_HDR_PREFIX)/include/unicode; \
					for i in *.h; do \
						if fgrep -q -x $$i $(SRCROOT)/minimalapis.txt ; then \
							mv $$i $(DEST_ROOT)$(HDR_PREFIX)/include/unicode ; \
						fi ; \
					done ); \
				$(CC) $(SRCROOT)/minimalapisTest.c $(INSTALLHDRS_ARCH) $(ISYSROOT) -nostdinc \
					-I $(DEST_ROOT)$(HDR_PREFIX)/include/ -I $(SDKROOT)/usr/include/ -E > /dev/null ; \
			fi; \
		fi; \
	);

# We run configure and run make first. This generates the .o files. We then link them 
# all up together into libicucore. Then we put it into its install location, create 
# symbolic links, and then strip the main dylib. Then install the remaining libraries. 
# We cleanup the sources folder.

install : installhdrsint icu
	if test ! -d $(DEST_ROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
	fi;
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuuc.lib $(DEST_ROOT)$(winlibdir)libicuuc.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuuc.pdb $(DEST_ROOT)$(libdir)libicuuc.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/libicuuc.dll $(DEST_ROOT)$(libdir)libicuuc.dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuin.lib $(DEST_ROOT)$(winlibdir)libicuin.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuin.pdb $(DEST_ROOT)$(libdir)libicuin.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/libicuin.dll $(DEST_ROOT)$(libdir)libicuin.dll; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icudt$(ICU_VERS).dll $(DEST_ROOT)$(libdir)icudt$(ICU_VERS).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/shim/icuuc.lib $(DEST_ROOT)$(winlibdir)icuuc.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuuc40.pdb $(DEST_ROOT)$(libdir)icuuc40.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuuc40.dll $(DEST_ROOT)$(libdir)icuuc40.dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/shim/icuin.lib $(DEST_ROOT)$(winlibdir)icuin.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/icuin40.pdb $(DEST_ROOT)$(libdir)icuin40.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/icuin40.dll $(DEST_ROOT)$(libdir)icuin40.dll; \
	else \
		$(INSTALL) -b -m 0755 $(OBJROOT)/$(INSTALLED_DYLIB) $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		if test "$(LINUX)" = "YES"; then \
			cp $(OBJROOT)/$(INSTALLED_DYLIB) $(SYMROOT)/$(INSTALLED_DYLIB); \
			strip -x -S $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		else \
			(cd $(DEST_ROOT)$(libdir); \
			ln -fs  $(INSTALLED_DYLIB) $(DYLIB)); \
			cp $(OBJROOT)/$(INSTALLED_DYLIB) $(SYMROOT)/$(INSTALLED_DYLIB); \
			$(STRIPCMD) -x -u -r -S $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		fi;	\
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
		if test ! -d $(DEST_ROOT)$(datadir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(datadir)/; \
		fi;	\
		if test -f $(OBJROOT)/$(L_DATA_FILE); then \
			$(INSTALL) -b -m 0644  $(OBJROOT)/$(L_DATA_FILE) $(DEST_ROOT)$(datadir)$(L_DATA_FILE); \
		fi; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/license.html $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)ICU.html; \
		if test ! -d $(DEST_ROOT)$(localtooldir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(localtooldir)/; \
		fi;	\
		if test -f $(OBJROOT)/$(INFOTOOL); then \
			$(INSTALL) -b -m 0755  $(OBJROOT)/$(INFOTOOL) $(DEST_ROOT)$(localtooldir)$(INFOTOOL); \
		fi; \
	fi;

DEPEND_install_debug = debug
DEPEND_install_profile = profile
	
.SECONDEXPANSION:
install_debug install_profile : $$(DEPEND_$$@)
	if test ! -d $(DEST_ROOT)$(libdir)/; then \
		$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
	fi;
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuuc_$(DEPEND_$@).lib $(DEST_ROOT)$(winlibdir)libicuuc_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuuc_$(DEPEND_$@).pdb $(DEST_ROOT)$(libdir)libicuuc_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/libicuuc_$(DEPEND_$@).dll $(DEST_ROOT)$(libdir)libicuuc_$(DEPEND_$@).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuin_$(DEPEND_$@).lib $(DEST_ROOT)$(winlibdir)libicuin_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT)/lib/libicuin_$(DEPEND_$@).pdb $(DEST_ROOT)$(libdir)libicuin_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT)/lib/libicuin_$(DEPEND_$@).dll $(DEST_ROOT)$(libdir)libicuin_$(DEPEND_$@).dll; \
	else \
		$(INSTALL) -b -m 0664 $(OBJROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		if test "$(LINUX)" = "YES"; then \
			cp $(OBJROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			strip -x -S $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		else \
			(cd $(DEST_ROOT)$(libdir); \
			ln -fs  $($(INSTALLED_DYLIB_$(DEPEND_$@))) $($(DYLIB_$(DEPEND_$@)))); \
			cp $(OBJROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			$(STRIPCMD) -x -u -r -S $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		fi;	\
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
	fi;

clean :
	-rm -rf $(OBJROOT)

