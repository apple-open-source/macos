##
# Wrapper makefile for ICU
# Copyright (C) 2003-2014 Apple Inc. All rights reserved.
#
# See http://www.gnu.org/manual/make/html_chapter/make_toc.html#SEC_Contents
# for documentation on makefiles. Most of this was culled from the ncurses makefile.
#
#################################
# Notes on building for AAS using Windows (7) + Visual Studio (2010) + Cygwin:
#
# Either this should be run indirectly from the VS command prompt via the
# BuildICUForAAS script or project, using the instructions there (which build
# both 32-bit and 64-bit), or it should be run from within Cygwin using the
# following instructions or equivalent (different steps for 32-bit or 64-bit):
#
# 1. From VS command prompt, run vcvarsall.bat to set various environment variables.
#    For a 32-bit build:
#    > "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86
#    For a 64-bit build:
#    > "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" amd64
#
# 2. Launch Cygwin, e.g.
#    > C:\cygwin\Cygwin.bat
#
# 3. Within cygwin, cd to the top level of the ICU sources directory, e.g.
#    $ cd ICU
#
# 4. Adjust the PATH to put the appropriate VC tools directory first:
#    For a 32-bit build:
#    $ export PATH="/cygdrive/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/":$PATH
#    For a 64-bit build:
#    $ export PATH="/cygdrive/c/Program Files (x86)/Microsoft Visual Studio 10.0/VC/bin/amd64/":$PATH
#
# 5. Run the ICU makefile
#    For a 32-bit build:
#    make [install] WINDOWS=YES [ARCH64=NO] [DSTROOT=...]
#    For a 64-bit build:
#    make [install] WINDOWS=YES ARCH64=YES [DSTROOT=...]
#
#################################

#################################
#################################
# MAKE VARS
#################################
#################################

# ':=' denotes a "simply expanded" variable. It's value is
# set at the time of definition and it never recursively expands
# when used. This is in contrast to using '=' which denotes a
# recursively expanded variable.

# Sane defaults, which are typically overridden on the command line.
WINDOWS=NO
LINUX=NO
ARCH64=NO

# chicken and egg problem: we can't use cygpath until PATH & SHELL are set,
# but we have to convert VS100VCTOOLS_PATH in order to set PATH. So instead we
# convert using subst.
ifeq "$(WINDOWS)" "YES"
	ifneq "$(VS100VCTOOLS_PATH)" ""
		VS100VCTOOLS_CYGPATH:= /cygdrive/$(subst :/,/,$(subst \,/,$(VS100VCTOOLS_PATH)))
		PATH:=$(VS100VCTOOLS_CYGPATH):/usr/local/bin/:/usr/bin/:$(PATH)
	endif
endif
$(info # PATH=$(PATH))

# For some reason, cygwin bash (at least when run non-login) needs to use
# bash for pwd, echo etc. (but uname does not work, see below)
ifeq "$(WINDOWS)" "YES"
	SHELL := /bin/bash
else
	SHELL := /bin/sh
endif

# if building for windows from batch script, convert Win-style paths
# for SRCROOT etc. to cygwin-style paths. Don't define them if not
# already defined.
ifeq "$(WINDOWS)" "YES"
	ifneq "$(VS100VCTOOLS_PATH)" ""
		ifdef SRCROOT
			SRCROOT:=$(shell /bin/cygpath -ua $(subst \,/,$(SRCROOT)))
		endif
		ifdef OBJROOT
			OBJROOT:=$(shell /bin/cygpath -ua $(subst \,/,$(OBJROOT)))
		endif
		ifdef DSTROOT
			DSTROOT:=$(shell /bin/cygpath -ua $(subst \,/,$(DSTROOT)))
		endif
		ifdef SYMROOT
			SYMROOT:=$(shell /bin/cygpath -ua $(subst \,/,$(SYMROOT)))
		endif
	endif
endif

ifndef SRCROOT
	SRCROOT:=$(shell pwd)
endif
ifndef OBJROOT
	OBJROOT:=$(SRCROOT)/build
endif
ifndef DSTROOT
	DSTROOT:=$(OBJROOT)
endif
ifndef SYMROOT
	SYMROOT:=$(OBJROOT)
endif

ifeq "$(WINDOWS)" "YES"
	ifeq "$(ARCH64)" "YES"
		OBJROOT_CURRENT=$(OBJROOT)/obj64
		SYMROOT_CURRENT=$(SYMROOT)/obj64
	else
		OBJROOT_CURRENT=$(OBJROOT)/obj32
		SYMROOT_CURRENT=$(SYMROOT)/obj32
	endif
else ifeq "$(LINUX)" "YES"
	ifeq "$(ARCH64)" "YES"
		OBJROOT_CURRENT=$(OBJROOT)/obj64
		SYMROOT_CURRENT=$(SYMROOT)/obj64
	else
		OBJROOT_CURRENT=$(OBJROOT)/obj32
		SYMROOT_CURRENT=$(SYMROOT)/obj32
	endif
else
	OBJROOT_CURRENT=$(OBJROOT)
	SYMROOT_CURRENT=$(SYMROOT)
endif
CROSSHOST_OBJROOT=$(OBJROOT)/crossbuildhost
APPLE_INTERNAL_DIR=/AppleInternal
RC_ARCHS=
MAC_OS_X_VERSION_MIN_REQUIRED=1070
OSX_HOST_VERSION_MIN_STRING=10.7
IOS_VERSION_TARGET_STRING=7.0
OSX_VERSION_TARGET_STRING=10.9

$(info # SRCROOT=$(SRCROOT))
$(info # DSTROOT=$(DSTROOT))
$(info # OBJROOT=$(OBJROOT))

# For some reason, under cygwin, bash uname is not found, and
# sh uname does not produce a result with -p or -m. So we just
# hardcode here.
ifeq "$(WINDOWS)" "YES"
	UNAME_PROCESSOR:=i386
else
	UNAME_PROCESSOR:=$(shell uname -p)
endif

ifeq "$(RC_INDIGO)" "YES"
	-include $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.indigo
	ifndef SDKROOT
		SDKROOT=$(INDIGO_PREFIX)
	endif
	DEST_ROOT=$(DSTROOT)/$(INDIGO_PREFIX)/
else
	DEST_ROOT=$(DSTROOT)/
endif
ifndef SDKROOT
	SDKPATH:=/
else ifeq "$(SDKROOT)" ""
	SDKPATH:=/
else
	SDKPATH:=$(shell xcodebuild -version -sdk $(SDKROOT) Path)
	ifeq "$(SDKPATH)" ""
		SDKPATH:=/
	endif
endif
ifneq "$(RC_ARCHS)" ""
	ifneq "$(filter arm armv6 armv7 armv7s arm64,$(RC_ARCHS))" ""
		CROSS_BUILD=YES
		BUILD_TYPE=DEVICE
	else ifeq "$(RC_INDIGO)" "YES"
		CROSS_BUILD=YES
		BUILD_TYPE=SIMULATOR
	else
		CROSS_BUILD=NO
		BUILD_TYPE=
	endif
	INSTALLHDRS_ARCH=-arch $(shell echo $(RC_ARCHS) | cut -d' ' -f1)
else
	CROSS_BUILD=NO
	INSTALLHDRS_ARCH=
	BUILD_TYPE=
endif
$(info # RC_ARCHS=$(RC_ARCHS))
$(info # INSTALLHDRS_ARCH=$(INSTALLHDRS_ARCH))
$(info # buildhost=$(UNAME_PROCESSOR))
$(info # SDKROOT=$(SDKROOT))
$(info # SDKPATH=$(SDKPATH))
$(info # RC_INDIGO=$(RC_INDIGO))
$(info # CROSS_BUILD=$(CROSS_BUILD))
$(info # BUILD_TYPE=$(BUILD_TYPE))
$(info # DEST_ROOT=$(DEST_ROOT))


# FORCEENDIAN below is to override silly configure behavior in which if
# __APPLE_CC__ is defined and archs are in { ppc, ppc64, i386, x86_64 }
# then it assumes a universal build (ac_cv_c_bigendian=universal) with
# data file initially built big-endian.
#
ifeq "$(CROSS_BUILD)" "YES"
	RC_ARCHS_FIRST=$(shell echo $(RC_ARCHS) | cut -d' ' -f1)
	TARGET_SPEC=$(RC_ARCHS_FIRST)-apple-darwin14.0.0
	ENV_CONFIGURE_ARCHS=-arch $(RC_ARCHS_FIRST)
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
	ICUPKGTOOLIBS="$(OBJROOT_CURRENT)/lib:$(OBJROOT_CURRENT)/stubdata"
	ICUPKGTOOL=$(OBJROOT_CURRENT)/bin/icupkg
	FORCEENDIAN=
else
	TARGET_SPEC=$(UNAME_PROCESSOR)-apple-darwin14.0.0
	ENV_CONFIGURE_ARCHS=
	ICUPKGTOOLIBS="$(OBJROOT_CURRENT)/lib:$(OBJROOT_CURRENT)/stubdata"
	ICUPKGTOOL=$(OBJROOT_CURRENT)/bin/icupkg
	FORCEENDIAN=
endif
$(info # TARGET_SPEC=$(TARGET_SPEC))
$(info # ENV_CONFIGURE_ARCHS=$(ENV_CONFIGURE_ARCHS))

ICU_TARGET_VERSION_FOR_TZ_EXTRA :=
ifeq "$(BUILD_TYPE)" "SIMULATOR"
	ICU_TARGET_VERSION_FOR_TZ_EXTRA := -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING)
endif

ifeq "$(BUILD_TYPE)" "DEVICE"
	THUMB_FLAG = -mthumb
else
	THUMB_FLAG =
endif

# even for a crossbuild host build, we want to use the target's latest tzdata as pointed to by latest_tzdata.tar.gz
export TZDATA:=$(SDKPATH)/usr/local/share/tz/$(shell readlink $(SDKPATH)/usr/local/share/tz/latest_tzdata.tar.gz)
$(info # TZDATA=$(TZDATA))

ifeq "$(WINDOWS)" "YES"
	EMBEDDED:=0
else ifeq "$(LINUX)" "YES"
	CC := gcc
	CXX := g++
	EMBEDDED:=0
	ISYSROOT =
else
	ifeq "$(BUILD_TYPE)" ""
		HOSTCC := $(shell xcrun -sdk $(SDKPATH) -find cc)
		HOSTCXX := $(shell xcrun -sdk $(SDKPATH) -find c++)
		ifeq "$(SDKPATH)" "/"
			ifneq (,$(findstring XcodeDefault,$(HOSTCC)))
				HOSTSDKPATH := $(shell xcodebuild -version -sdk macosx Path)
			else
				HOSTSDKPATH := $(shell xcodebuild -version -sdk macosx.internal Path)
			endif
		else
			HOSTSDKPATH := $(SDKPATH)
		endif
		ISYSROOT:= -isysroot $(HOSTSDKPATH)
		CC := $(HOSTCC)
		CXX := $(HOSTCXX)
		NM := $(shell xcrun -find nm)
		STRIPCMD := $(shell xcrun -find strip)
	else
		HOSTCC := $(shell xcrun -sdk macosx -find cc)
		HOSTCXX := $(shell xcrun -sdk macosx -find c++)
		HOSTSDKPATH := $(shell xcodebuild -version -sdk macosx Path)
		ISYSROOT:= -isysroot $(SDKPATH)
		CC := $(shell xcrun -sdk $(SDKPATH) -find cc)
		CXX := $(shell xcrun -sdk $(SDKPATH) -find c++)
		NM := $(shell xcrun -sdk $(SDKPATH) -find nm)
		STRIPCMD := $(shell xcrun -sdk $(SDKPATH) -find strip)
	endif
	HOSTISYSROOT = -isysroot $(HOSTSDKPATH)
	EMBEDDED:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep TARGET_OS_EMBEDDED | cut -d' ' -f3)
endif
DSYMTOOL := /usr/bin/dsymutil
DSYMSUFFIX := .dSYM

$(info # HOSTCC=$(HOSTCC))
$(info # HOSTCXX=$(HOSTCXX))
$(info # HOSTISYSROOT=$(HOSTISYSROOT))
$(info # CC=$(CC))
$(info # CXX=$(CXX))
$(info # ISYSROOT=$(ISYSROOT))

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
ifeq "$(ICU_BUILD)" "0"
	DEFINE_BUILD_LEVEL = 
else
	DEFINE_BUILD_LEVEL =-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)
endif

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
	ifeq "$(ARCH64)" "YES"
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples --disable-icuio \
			--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=64 \
			$(DRAFT_FLAG)
	else
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples --disable-icuio \
			--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=32 \
			$(DRAFT_FLAG)
	endif
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
ICU_VERS = 53
ICU_SUBVERS = 1
CORE_VERS = A

ifeq "$(WINDOWS)" "YES"
	DYLIB_SUFF = dll
	ifeq "$(ARCH64)" "YES"
		winprogdir = /Program\ Files/Common\ Files/Apple/Apple\ Application\ Support/
		winintlibdir = /AppleInternal/lib64/
	else
		winprogdir = /Program\ Files\ \(x86\)/Common\ Files/Apple/Apple\ Application\ Support/
		winintlibdir = /AppleInternal/lib32/
	endif
	libdir =
else ifeq "$(LINUX)" "YES"
	DYLIB_SUFF = so
	ifeq "$(ARCH64)" "YES"
		libdir = /usr/lib64/
	else
		libdir = /usr/lib/
	endif
	winprogdir =
	winintlibdir =
else
	DYLIB_SUFF = dylib
	libdir = /usr/lib/
	winprogdir =
	winintlibdir =
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

OPEN_SOURCE_VERSIONS_DIR=/usr/local/OpenSourceVersions/
OPEN_SOURCE_LICENSES_DIR=/usr/local/OpenSourceLicenses/

B_DATA_FILE=icudt$(ICU_VERS)b.dat
L_DATA_FILE=icudt$(ICU_VERS)l.dat
DATA_BUILD_SUBDIR= data/out
DATA_INSTALL_DIR=/usr/share/icu/
# DATA_LOOKUP_DIR is what the target ICU_DATA_DIR gets set to in CFLAGS, CXXFLAGS;
# DATA_LOOKUP_DIR_BUILDHOST is what any crossbuild host ICU_DATA_DIR gets set to.
ifneq "$(APPLE_EMBEDDED)" "YES"
	DATA_LOOKUP_DIR=/usr/share/icu/
else
	ifeq "$(RC_INDIGO)" "YES"
		DATA_LOOKUP_DIR=/usr/share/icu/
	else
		DATA_LOOKUP_DIR=/var/db/icu/
	endif
endif
DATA_LOOKUP_DIR_BUILDHOST=/usr/share/icu/

# Name of runtime environment variable to get the prefix path for DATA_LOOKUP_DIR
# Currently we are only using this for LINUX, should also use for iOS simulator
ifeq "$(LINUX)" "YES"
	DATA_DIR_PREFIX_ENV_VAR=APPLE_FRAMEWORKS_ROOT
else
	DATA_DIR_PREFIX_ENV_VAR=
endif

#################################
# Info tool
#################################

localtooldir=/usr/local/bin/

INFOTOOL = icuinfo

INFOTOOL_OBJS = ./tools/icuinfo/icuinfo.o ./tools/toolutil/udbgutil.o ./tools/toolutil/uoptions.o

#################################
# CLDR file(s)
# e.g. supplementalData.xml, per <rdar://problem/13426014>
# These are like internal headers in that they are only installed for
# (other) projects to build against, not needed at runtime.
# Therefore install during installhdrs.
#################################

CLDRFILESDIR=/usr/local/share/cldr

#################################
# Environment variables
#################################

# $(RC_ARCHS:%=-arch %) is a substitution reference. It denotes, in this case,
# for every value <val> in RC_ARCHS, replace it with "-arch <val>". Substitution
# references have the form $(var:a=b). We can insert the strip and prebinding commands
# into CFLAGS (and CXXFLAGS). This controls a lot of the external variables so we don't
# need to directly modify the ICU files (like for CFLAGS, etc).

LIBOVERRIDES=LIBICUDT="-L$(OBJROOT_CURRENT) -l$(LIB_NAME)" \
	LIBICUUC="-L$(OBJROOT_CURRENT) -l$(LIB_NAME)" \
	LIBICUI18N="-L$(OBJROOT_CURRENT) -l$(LIB_NAME)"

# For normal Windows builds set the ENV= options here; for debug Windows builds set the ENV_DEBUG=
# options here and also the update the LINK.EXE lines in the TARGETS section below.
ifeq "$(WINDOWS)" "YES"
	ifeq "$(ARCH64)" "YES"
		ENV= CFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
			CXXFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t" \
			LDFLAGS="/NXCOMPAT /DYNAMICBASE /DEBUG /OPT:REF"
	else
		ENV= CFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
			CXXFLAGS="/O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t" \
			LDFLAGS="/NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF"
	endif
	ENV_CONFIGURE= CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)" \
		CFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
		CXXFLAGS="/O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t"
	ENV_DEBUG= CFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
		CXXFLAGS="/O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc" \
		LDFLAGS="/DEBUG /DYNAMICBASE"
	ENV_PROFILE=
else ifeq "$(LINUX)" "YES"
	ifeq "$(ARCH64)" "YES"
		ENV_CONFIGURE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)"  \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib64"

		ENV= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib64"

		ENV_DEBUG= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib64"
		
		ENV_PROFILE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib64"
	else
		ENV_CONFIGURE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
		
		ENV= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
			
		ENV_DEBUG= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
		
		ENV_PROFILE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
	endif
else
	CPPOPTIONS = 
	ENV_CONFIGURE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) $(ISYSROOT) $(ENV_CONFIGURE_ARCHS)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" $(FORCEENDIAN)\
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
	
	ENV= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
		
	ENV_DEBUG= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
	
	ENV_PROFILE= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(ISYSROOT) $(THUMB_FLAG)" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"

	ENV_CONFIGURE_BUILDHOST= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) $(HOSTISYSROOT)" \
		CC="$(HOSTCC)" \
		CXX="$(HOSTCXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"
	
	ENV_BUILDHOST= APPLE_INTERNAL_DIR="$(APPLE_INTERNAL_DIR)" \
		CC="$(HOSTCC)" \
		CXX="$(HOSTCXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
		CXXFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DEST_ROOT)usr/local/lib"

endif
	
ENV_icu = ENV
ENV_debug = ENV_DEBUG
ENV_profile = ENV_PROFILE

ifeq "$(BUILD_TYPE)" "DEVICE"
	ORDERFILE=$(SDKPATH)/AppleInternal/OrderFiles/libicucore.order
else ifeq "$(APPLE_EMBEDDED)" "YES"
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

icu debug profile : $(OBJROOT_CURRENT)/Makefile
	echo "# make for target";
	(cd $(OBJROOT_CURRENT); \
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
				(cd common; \
					rm -f ../lib/libicuuc.dll.manifest; \
					if test "$(ARCH64)" = "YES"; then \
						LINK.EXE /DLL /NXCOMPAT /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
							/IMPLIB:../lib/libicuuc.lib /out:../lib/libicuuc.dll \
							*.o libicuuc.res ../stubdata/icudt.lib advapi32.lib; \
					else \
						LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
							/IMPLIB:../lib/libicuuc.lib /out:../lib/libicuuc.dll \
							*.o libicuuc.res ../stubdata/icudt.lib advapi32.lib; \
					fi; \
					mt.exe -nologo -manifest ../lib/libicuuc.dll.manifest -outputresource:"../lib/libicuuc.dll;2"; \
				); \
				(cd i18n; \
					rm -f ../lib/libicuin.dll.manifest; \
					if test "$(ARCH64)" = "YES"; then \
						LINK.EXE /DLL /NXCOMPAT /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
							/IMPLIB:../lib/libicuin.lib /out:../lib/libicuin.dll \
							*.o libicuin.res ../lib/libicuuc.lib ../stubdata/icudt.lib advapi32.lib; \
					else \
						LINK.EXE /DLL /NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF /MANIFEST \
							/IMPLIB:../lib/libicuin.lib /out:../lib/libicuin.dll \
							*.o libicuin.res ../lib/libicuuc.lib ../stubdata/icudt.lib advapi32.lib; \
					fi; \
					mt.exe -nologo -manifest ../lib/libicuin.dll.manifest -outputresource:"../lib/libicuin.dll;2"; \
				); \
				if test "$(ARCH64)" != "YES"; then \
					mkdir -p lib/shim; \
					(cd common; \
						rm -f icuuc40shim.o; \
						rm -f ../lib/icuuc40.dll.manifest; \
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
						rm -f ../lib/icuin40.dll.manifest; \
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
						$(LDFLAGS) -dead_strip -o ./$(INFOTOOL) $(INFOTOOL_OBJS) -L./ -l$(LIB_NAME) ; \
				fi; \
			fi; \
			if test -f ./$(DATA_BUILD_SUBDIR)/$(B_DATA_FILE); then \
				ln -fs ./$(DATA_BUILD_SUBDIR)/$(B_DATA_FILE); \
			fi; \
			if test -f ./$(DATA_BUILD_SUBDIR)/$(L_DATA_FILE); then \
				ln -fs ./$(DATA_BUILD_SUBDIR)/$(L_DATA_FILE); \
			else \
				DYLD_LIBRARY_PATH=$(ICUPKGTOOLIBS) \
				$(ICUPKGTOOL) -tl ./$(DATA_BUILD_SUBDIR)/$(B_DATA_FILE) $(L_DATA_FILE); \
			fi; \
		fi; \
	);

crossbuildhost : $(CROSSHOST_OBJROOT)/Makefile
	echo "# make for crossbuild host";
	(cd $(CROSSHOST_OBJROOT); \
		$(MAKE) $($(ENV_BUILDHOST)); \
	);

check : icu
ifneq "$(CROSS_BUILD)" "YES"
	(cd $(OBJROOT_CURRENT); \
		ICU_DATA=$(OBJROOT_CURRENT) $(MAKE) $(ENV) check; \
	);
else
	$(warning check not supported for cross-build)
endif

check-debug: debug
ifneq "$(CROSS_BUILD)" "YES"
	(cd $(OBJROOT_CURRENT); \
		ICU_DATA=$(OBJROOT_CURRENT) $(MAKE) $(ENV_DEBUG) check; \
	);
else
	$(warning check not supported for cross-build)
endif

samples: icu
	(cd $(OBJROOT_CURRENT)/samples; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

extra: icu
	(cd $(OBJROOT_CURRENT)/extra; \
		$(MAKE) $(ENV_DEBUG) $(LIBOVERRIDES); \
	);

ifneq "$(CROSS_BUILD)" "YES"
$(OBJROOT_CURRENT)/Makefile : 
else
$(OBJROOT_CURRENT)/Makefile : crossbuildhost
endif
	if test ! -d $(OBJROOT_CURRENT); then \
		mkdir -p $(OBJROOT_CURRENT); \
	fi;
	cp -Rpf $(SRCROOT)/icuSources/* $(OBJROOT_CURRENT)/;
	if test "$(WINDOWS)" = "YES"; then \
		echo "# configure for target"; \
		(cd $(OBJROOT_CURRENT)/data/unidata; mv base_unidata/*.txt .;); \
		(cd $(OBJROOT_CURRENT)/data/unidata/norm2; mv base_norm2/*.txt .;); \
		(cd $(OBJROOT_CURRENT)/data/in; mv base_in/*.nrm .; mv base_in/*.icu .;); \
		(cd $(OBJROOT_CURRENT); $(ENV_CONFIGURE) ./runConfigureICU Cygwin/MSVC $(CONFIG_FLAGS);) \
	elif test "$(LINUX)" = "YES"; then \
		echo "# configure for target"; \
		(cd $(OBJROOT_CURRENT)/data/unidata; mv base_unidata/*.txt .;); \
		(cd $(OBJROOT_CURRENT)/data/unidata/norm2; mv base_norm2/*.txt .;); \
		(cd $(OBJROOT_CURRENT)/data/in; mv base_in/*.nrm .; mv base_in/*.icu .;); \
		(cd $(OBJROOT_CURRENT); $(ENV_CONFIGURE) ./runConfigureICU Linux $(CONFIG_FLAGS);) \
	elif test "$(CROSS_BUILD)" = "YES"; then \
		echo "# configure for crossbuild target"; \
		(cd $(OBJROOT_CURRENT); $(ENV_CONFIGURE) ./configure --host=$(TARGET_SPEC) --with-cross-build=$(CROSSHOST_OBJROOT) $(CONFIG_FLAGS);) \
	else \
		echo "# configure for non-crossbuild target"; \
		(cd $(OBJROOT_CURRENT); $(ENV_CONFIGURE) ./runConfigureICU MacOSX $(CONFIG_FLAGS);) \
	fi;
	if test "$(APPLE_EMBEDDED)" = "YES"; then \
		(cd $(OBJROOT_CURRENT)/common/unicode/; patch <$(SRCROOT)/minimalpatchconfig.txt;) \
	elif test "$(WINDOWS)" = "YES"; then \
		(cd $(OBJROOT_CURRENT)/common/unicode/; patch <$(SRCROOT)/windowspatchconfig.txt;) \
	else \
		(cd $(OBJROOT_CURRENT)/common/unicode/; patch <$(SRCROOT)/patchconfig.txt;) \
	fi; \

# for the tools that build the data file, cannot set UDATA_DEFAULT_ACCESS = UDATA_ONLY_PACKAGES
# as minimalpatchconfig.txt does; need different patches for the host build.
$(CROSSHOST_OBJROOT)/Makefile : 
	if test ! -d $(CROSSHOST_OBJROOT); then \
		mkdir -p $(CROSSHOST_OBJROOT); \
	fi;
	cp -Rpf $(SRCROOT)/icuSources/* $(CROSSHOST_OBJROOT);
	echo "# configure for crossbuild host";
	(cd $(CROSSHOST_OBJROOT); $(ENV_CONFIGURE_BUILDHOST) ./runConfigureICU MacOSX $(CONFIG_FLAGS);)
	(cd $(CROSSHOST_OBJROOT)/common/unicode/; patch <$(SRCROOT)/crosshostpatchconfig.txt;)

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
	tar cf - ./makefile ./ICU.plist ./license.html ./icuSources ./cldrFiles $(INSTALLSRC_VARFILES) | (cd $(SRCROOT) ; tar xfp -); \
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
	
installhdrsint : $(OBJROOT_CURRENT)/Makefile
	(cd $(OBJROOT_CURRENT); \
		for subdir in $(HDR_MAKE_SUBDIR); do \
			(cd $$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-headers); \
		done; \
		if test "$(APPLE_EMBEDDED)" = "YES"; then \
			if test ! -d $(DEST_ROOT)$(HDR_PREFIX)/include/unicode/; then \
				$(INSTALL) -d -m 0755 $(DEST_ROOT)$(HDR_PREFIX)/include/unicode/; \
			fi; \
			if test -d $(DEST_ROOT)$(PRIVATE_HDR_PREFIX)/include/unicode/; then \
				(cd $(DEST_ROOT)$(PRIVATE_HDR_PREFIX)/include/unicode; \
					for i in *.h; do \
						if fgrep -q -x $$i $(SRCROOT)/minimalapis.txt ; then \
							mv $$i $(DEST_ROOT)$(HDR_PREFIX)/include/unicode ; \
						fi ; \
					done ); \
				$(CC) $(SRCROOT)/minimalapisTest.c $(INSTALLHDRS_ARCH) $(ISYSROOT) -nostdinc \
					-I $(DEST_ROOT)$(HDR_PREFIX)/include/ -I $(SDKPATH)/usr/include/ -E > /dev/null ; \
			fi; \
			if test ! -d $(DEST_ROOT)$(CLDRFILESDIR)/; then \
				$(INSTALL) -d -m 0755 $(DEST_ROOT)$(CLDRFILESDIR)/; \
			fi; \
			$(INSTALL) -b -m 0644  $(SRCROOT)/cldrFiles/supplementalData.xml $(DEST_ROOT)$(CLDRFILESDIR)/supplementalData.xml; \
		fi; \
	);

# We run configure and run make first. This generates the .o files. We then link them 
# all up together into libicucore. Then we put it into its install location, create 
# symbolic links, and then strip the main dylib. Then install the remaining libraries. 
# We cleanup the sources folder.

install : installhdrsint icu
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winprogdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winprogdir)/; \
		fi; \
		if test ! -d $(DEST_ROOT)$(winintlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winintlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc.lib $(DEST_ROOT)$(winintlibdir)libicuuc.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc.pdb $(DEST_ROOT)$(winprogdir)libicuuc.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuuc.dll $(DEST_ROOT)$(winprogdir)libicuuc.dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin.lib $(DEST_ROOT)$(winintlibdir)libicuin.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin.pdb $(DEST_ROOT)$(winprogdir)libicuin.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuin.dll $(DEST_ROOT)$(winprogdir)libicuin.dll; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/icudt$(ICU_VERS).dll $(DEST_ROOT)$(winprogdir)icudt$(ICU_VERS).dll; \
		if test "$(ARCH64)" != "YES"; then \
			$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/shim/icuuc.lib $(DEST_ROOT)$(winintlibdir)icuuc.lib; \
			$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/icuuc40.pdb $(DEST_ROOT)$(winprogdir)icuuc40.pdb; \
			$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/icuuc40.dll $(DEST_ROOT)$(winprogdir)icuuc40.dll; \
			$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/shim/icuin.lib $(DEST_ROOT)$(winintlibdir)icuin.lib; \
			$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/icuin40.pdb $(DEST_ROOT)$(winprogdir)icuin40.pdb; \
			$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/icuin40.dll $(DEST_ROOT)$(winprogdir)icuin40.dll; \
		fi; \
	else \
		if test ! -d $(DEST_ROOT)$(libdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
		fi; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		if test "$(LINUX)" = "YES"; then \
			cp $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			strip -x -S $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		else \
			(cd $(DEST_ROOT)$(libdir); \
			ln -fs  $(INSTALLED_DYLIB) $(DYLIB)); \
			cp $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			if test "$(APPLE_EMBEDDED)" = "NO"; then \
				$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			fi; \
			$(STRIPCMD) -x -u -r -S $(DEST_ROOT)$(libdir)$(INSTALLED_DYLIB); \
		fi; \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT_CURRENT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
		if test ! -d $(DEST_ROOT)$(DATA_INSTALL_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(DATA_INSTALL_DIR)/; \
		fi; \
		if test -f $(OBJROOT_CURRENT)/$(L_DATA_FILE); then \
			$(INSTALL) -b -m 0644  $(OBJROOT_CURRENT)/$(L_DATA_FILE) $(DEST_ROOT)$(DATA_INSTALL_DIR)$(L_DATA_FILE); \
		fi; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DEST_ROOT)$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist; \
		if test ! -d $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/license.html $(DEST_ROOT)$(OPEN_SOURCE_LICENSES_DIR)ICU.html; \
		if test -f $(OBJROOT_CURRENT)/$(INFOTOOL); then \
			if test ! -d $(DEST_ROOT)$(localtooldir)/; then \
				$(INSTALL) -d -m 0755 $(DEST_ROOT)$(localtooldir)/; \
			fi; \
			$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(INFOTOOL) $(DEST_ROOT)$(localtooldir)$(INFOTOOL); \
			if test "$(LINUX)" != "YES"; then \
				cp $(OBJROOT_CURRENT)/$(INFOTOOL) $(SYMROOT_CURRENT)/$(INFOTOOL); \
				if test "$(APPLE_EMBEDDED)" = "NO"; then \
					$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(INFOTOOL)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(INFOTOOL); \
				fi; \
			fi; \
		fi; \
	fi;

DEPEND_install_debug = debug
DEPEND_install_profile = profile
	
.SECONDEXPANSION:
install_debug install_profile : $$(DEPEND_$$@)
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DEST_ROOT)$(winprogdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winprogdir)/; \
		fi; \
		if test ! -d $(DEST_ROOT)$(winintlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(winintlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).lib $(DEST_ROOT)$(winintlibdir)libicuuc_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).pdb $(DEST_ROOT)$(winprogdir)libicuuc_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).dll $(DEST_ROOT)$(winprogdir)libicuuc_$(DEPEND_$@).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).lib $(DEST_ROOT)$(winintlibdir)libicuin_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).pdb $(DEST_ROOT)$(winprogdir)libicuin_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).dll $(DEST_ROOT)$(winprogdir)libicuin_$(DEPEND_$@).dll; \
	else \
		if test ! -d $(DEST_ROOT)$(libdir)/; then \
			$(INSTALL) -d -m 0755 $(DEST_ROOT)$(libdir)/; \
		fi; \
		$(INSTALL) -b -m 0664 $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		if test "$(LINUX)" = "YES"; then \
			cp $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			strip -x -S $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		else \
			(cd $(DEST_ROOT)$(libdir); \
			ln -fs  $($(INSTALLED_DYLIB_$(DEPEND_$@))) $($(DYLIB_$(DEPEND_$@)))); \
			cp $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			$(STRIPCMD) -x -u -r -S $(DEST_ROOT)$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		fi; \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT_CURRENT)/$$subdir; $(MAKE) -e DESTDIR=$(DEST_ROOT) $(ENV) install-library;) \
		done; \
	fi;

clean :
	-rm -rf $(OBJROOT)

