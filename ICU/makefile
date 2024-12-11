##
# Wrapper makefile for ICU
# Copyright (C) 2003-2020 Apple Inc. All rights reserved.
#
# See http://www.gnu.org/manual/make/html_chapter/make_toc.html#SEC_Contents
# for documentation on makefiles. Most of this was culled from the ncurses makefile.
#
#################################
# Apple ICU repository tag and submission numbers
#
# The repository tag number consists of a main version and an optional branch version, as
# follows:
# ICU-MMmAA[.B]
#
# MM comes from the open-source ICU major version, and can range from 1 to 214 (the max is
#    due to B&I limits on submission versions). As of March 2016 this is 57; it increases
#    by 1 for each ICU major release (usually 2 / year). This corresponds to
#    U_ICU_VERSION_MAJOR_NUM.
# m  is a single digit specifying the open-source ICU minor version. This is 0 before final
#    release (e.g. for pre-release milestones), becomes 1 for release candidate and final
#    release, and may increase to 2 or a little more for occasional dot releases with
#    maintenance fixes. This corresponds to U_ICU_VERSION_MINOR_NUM.
#
#    Note that the ICU release may also utilize U_ICU_VERSION_PATCHLEVEL_NUM; however
#    that is ignored for the Apple ICU tag and submission numbers, and any related code
#    or data changes will bne distinguished by updating the Apple delta version AA below.
#
# AA is a 2-digit value specifying the version of the additional Apple deltas after
#    integrating a major release. This is always 2 digits (zero padded) and ranges from
#    00 (the first release after integrating a new ICU version) to 99.
# B  is an optional branch version, i.e. a branch from the mainline version specified by
#    the MMmAA part. It can range from 1 to 99 and is NOT zero-padded.
#
#    For Windows builds, the first 3 values of the 4-part FILEVERSION and PRODUCTVERSION
#    are formed from U_ICU_VERSION_MAJOR_NUM, U_ICU_VERSION_MINOR_NUM, and
#    U_ICU_VERSION_PATCHLEVEL_NUM; the 4th value is ICU_BUILD, formed from AA and B as
#    ICU_BUILD = 100*AA + B.
#
#################################
# The following is obsolete after ICU-66002:
#
# The B&I submission version always has 3 parts separated by '.':
# MMmAA.(0 | B).T
#
# The first part MMmAA is the same as the repository tag number.
# If the repository tag has no branch version, the second part is 0, otherwise it is
# the same as the branch version (1..99).
# The third part is a single digit designating the submission train; since the installsrc
# target now submits different sources to different trains, we have to distinguish them.
# The values currently defined for T are
# 1  OSX trains
# 2  embedded trains (iOS, tvos, watchos, bridgeos) including simulator versions thereof
# 8  AAS for Windows
# 9  linux for Siri servers
# (additional train numbers for Apple platforms can be assigned from 3 up, additional
# train numbers for non-Apple platforms can be assigned from 7 down).
#
#################################
# Notes on building for AAS using Windows (10) + Visual Studio (2015) + Cygwin:
#
# Either this should be run indirectly from the VS command prompt via the
# BuildICUForAAS script or project, using the instructions there (which build
# both 32-bit and 64-bit), or it should be run from within Cygwin using the
# following instructions or equivalent (different steps for 32-bit or 64-bit
# targets, details may also differ if you have the 32-bit cygwin install vs the
# 64-bit one, i.e. cygwin vs cygwin64):
#
# 1. From VS command prompt, run vcvarsall.bat to set various environment variables.
#    For a 32-bit build:
#    > "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86
#    For a 64-bit build:
#    > "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
#
# 2. Launch Cygwin, e.g. for the 64-bit cygwin install:
#    > C:\cygwin64\cygwin.bat
#
# 3. Within cygwin, cd to the top level of the ICU sources directory, e.g.
#    $ cd ICU
#
# 4. Adjust the PATH to put the appropriate VC tools directory first:
#    For a 32-bit build:
#    $ export PATH="/cygdrive/c/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/":$PATH
#    For a 64-bit build:
#    $ export PATH="/cygdrive/c/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/amd64/":$PATH
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

# Sane defaults, which are typically overridden on the command line
#or by the environment
ifeq ($(OS),Windows_NT)
	WINDOWS=YES
	LINUX=NO
else
	WINDOWS=NO
	LINUX?=$(shell [[ "`uname -s`" == "Linux" ]] && echo YES || echo NO)
endif
ARCH64?=YES
RC_ARCHS=
ifndef RC_ProjectSourceVersion
ifdef RC_PROJECTSOURCEVERSION
	RC_ProjectSourceVersion=$(RC_PROJECTSOURCEVERSION)
endif
endif
$(info # RC_XBS=$(RC_XBS))
$(info # RC_ARCHS=$(RC_ARCHS))
$(info # RC_ProjectName=$(RC_ProjectName))
$(info # RC_ProjectSourceVersion=$(RC_ProjectSourceVersion))

# Disallow $(WINDOWS) and $(LINUX) both YES
ifeq "$(WINDOWS)" "YES"
	ifeq "$(LINUX)" "YES"
		$(error WINDOWS and LINUX cannot both be YES)
	endif
endif

# chicken and egg problem: we can't use cygpath until PATH & SHELL are set,
# but we have to convert VS140VCTOOLS_PATH in order to set PATH. So instead we
# convert using subst.
ifeq "$(WINDOWS)" "YES"
	ifneq "$(VS140VCTOOLS_PATH)" ""
		VS140VCTOOLS_CYGPATH:= /cygdrive/$(subst :/,/,$(subst \,/,$(VS140VCTOOLS_PATH)))
		PATH:=$(VS140VCTOOLS_CYGPATH):/usr/local/bin/:/usr/bin/:$(PATH)
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
	ifneq "$(VS140VCTOOLS_PATH)" ""
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
$(info # SRCROOT=$(SRCROOT))
$(info # OBJROOT=$(OBJROOT))
$(info # DSTROOT=$(DSTROOT))

# Disallow $(SRCROOT) == $(OBJROOT)
ifeq ($(OBJROOT), $(SRCROOT))
$(error SRCROOT same as OBJROOT)
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
$(info # SDKROOT=$(SDKROOT))
$(info # SDKPATH=$(SDKPATH))

# An Apple submission version (passed in RC_ProjectSourceVersion for official builds) is
# X[.Y[.Z]]
# where X is in range 0-214747, Y and Z are in range 0-99 (with no leading zeros).
# This corresponds to MMmAA.(0 | B).T where
# MM is U_ICU_VERSION_MAJOR_NUM
# m  is U_ICU_VERSION_MINOR_NUM
# AA is the Apple delta version
# B  is the Apple branch version (1 or 2 digits)
# T  is the Apple train code for submissions. [obsolete after ICU-66002]
# Note The value for the SourceVersion property in version.plists will be calculated as
# (X*10000 + Y*100 + Z).
# We want ICU_BUILD = 100*AA + B.
#
ifneq "$(RC_ProjectSourceVersion)" ""
	ifeq "$(WINDOWS)" "YES"
		ICU_BUILD_AA  := $(shell echo $(RC_ProjectSourceVersion) | /usr/bin/sed -r -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\2/')
		ICU_BUILD_B1 := $(shell echo $(RC_ProjectSourceVersion) | /usr/bin/sed -r -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\4/')
		ICU_BUILD_B2 := $(shell echo $(RC_ProjectSourceVersion) | /usr/bin/sed -r -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\5/')
		ICU_TRAIN_CODE := $(shell echo $(RC_ProjectSourceVersion) | /usr/bin/sed -r -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\7/')
	else
		ICU_BUILD_AA  := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\2/')
		ICU_BUILD_B1 := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\4/')
		ICU_BUILD_B2 := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\5/')
		ICU_TRAIN_CODE := $(shell echo $(RC_ProjectSourceVersion) | sed -E -e 's/([0-9]+)([0-9]{2})(\.([0-9])([0-9])?(\.([0-9]{1,2}))?)?/\7/')
	endif
	ifeq "$(ICU_BUILD_AA)" ""
		ICU_BUILD := 0
	else
		ICU_BUILD := $(subst a,$(ICU_BUILD_AA),abc)
		ifeq "$(ICU_BUILD_B1)" ""
			ICU_BUILD := $(subst b,0,$(ICU_BUILD))
			ICU_BUILD := $(subst c,0,$(ICU_BUILD))
		else
			ifeq "$(ICU_BUILD_B2)" ""
				ICU_BUILD := $(subst b,0,$(ICU_BUILD))
				ICU_BUILD := $(subst c,$(ICU_BUILD_B1),$(ICU_BUILD))
			else
				ICU_BUILD := $(subst b,$(ICU_BUILD_B1),$(ICU_BUILD))
				ICU_BUILD := $(subst c,$(ICU_BUILD_B2),$(ICU_BUILD))
			endif
		endif
	endif
	ifeq "$(ICU_TRAIN_CODE)" ""
		ICU_TRAIN_CODE := 0
	endif
else
	ICU_BUILD := 0
	ICU_TRAIN_CODE := 0
endif
$(info # ICU_BUILD=$(ICU_BUILD))
$(info # ICU_TRAIN_CODE=$(ICU_TRAIN_CODE))
ifeq "$(ICU_BUILD)" "0"
	DEFINE_BUILD_LEVEL =
else
	DEFINE_BUILD_LEVEL =-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)
endif

# Determine build type. In some cases (e.g. running installsrc for submitproject)
# the only accurate information we may have about build type is from ICU_TRAIN_CODE,
# so give priority to that if nonzero. [though it is mostly obsolete after ICU-66002
# it might possibly be used for non-Apple-platform builds]
# The values currently defined for ICU_TRAIN_CODE and corresponding ICU_BUILD_TYPE are
# 1  OSX trains
# 2  embedded trains (iOS, tvos, watchos) including simulator versions thereof.
#    may have BUILD_TYPE=DEVICE,SIMULATOR,TOOL
# 8  AAS for Windows
# 9  linux for Siri servers
#
ifeq "$(WINDOWS)" "YES"
	ICU_FOR_APPLE_PLATFORMS:=NO
else ifeq "$(ICU_TRAIN_CODE)" "8"
	override WINDOWS=YES
	ICU_FOR_APPLE_PLATFORMS:=NO
else ifeq "$(LINUX)" "YES"
	ICU_FOR_APPLE_PLATFORMS:=NO
else ifeq "$(ICU_TRAIN_CODE)" "9"
	override LINUX=YES
	ICU_FOR_APPLE_PLATFORMS:=NO
else
	ICU_FOR_APPLE_PLATFORMS:=YES
endif

# For Apple builds, get more details from the SDK if available
# In TargetConditionals.h:
# TARGET_OS_IPHONE -is 1 iff generating code for firmware, devices, or simulator (all embedded trains)
# in that case:
# 	exactly one of the following is 1: TARGET_OS_SIMULATOR, TARGET_OS_EMBEDDED (i.e. device)
# 	exactly one of the following is 1: TARGET_OS_IOS, TARGET_OS_TV, TARGET_OS_WATCH, TARGET_OS_BRIDGE
#
# if SDKPATH is / then we are doing something like a make check against host SDK and not building
# for embedded, so ICU_FOR_EMBEDDED_TRAINS must be NO
#
ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
	HOSTCC := $(shell xcrun --sdk macosx --find cc)
	HOSTCXX := $(shell xcrun --sdk macosx --find c++)
	HOSTSDKPATH := $(shell xcrun --sdk macosx.internal --show-sdk-path)
	HOSTISYSROOT = -isysroot $(HOSTSDKPATH)
	ifeq "$(SDKPATH)" "/"
		ISYSROOT:= -isysroot $(HOSTSDKPATH)
		CC := $(HOSTCC)
		CXX := $(HOSTCXX)
		NM := $(shell xcrun --sdk macosx --find nm)
		STRIPCMD := $(shell xcrun --sdk macosx --find strip)
		export ICU_FOR_EMBEDDED_TRAINS:=NO
	else
		ISYSROOT:= -isysroot $(SDKPATH)
		CC := $(shell xcrun --sdk $(SDKPATH) --find cc)
		CXX := $(shell xcrun --sdk $(SDKPATH) --find c++)
		NM := $(shell xcrun --sdk $(SDKPATH) --find nm)
		STRIPCMD := $(shell xcrun --sdk $(SDKPATH) --find strip)
		EMBEDDEDTRAIN:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep define' 'TARGET_OS_IPHONE | cut -d' ' -f3)
		ifeq "$(EMBEDDEDTRAIN)" "1"
			export ICU_FOR_EMBEDDED_TRAINS:=YES
		else
		    export ICU_FOR_EMBEDDED_TRAINS:=NO
		endif
	endif
	SIMULATOROS:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep define' 'TARGET_OS_SIMULATOR | cut -d' ' -f3)
	TVOS:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep define' 'TARGET_OS_TV | cut -d' ' -f3)
	WATCHOS:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep define' 'TARGET_OS_WATCH | cut -d' ' -f3)
	BRIDGEOS:=$(shell $(CXX) -E -dM -x c $(ISYSROOT) -include TargetConditionals.h /dev/null | fgrep define' 'TARGET_OS_BRIDGE | cut -d' ' -f3)
	ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
		ifeq "$(SIMULATOROS)" "1"
			BUILD_TYPE=SIMULATOR
		else
			BUILD_TYPE=DEVICE
		endif
	else ifeq "$(RC_ProjectName)" "tzTools"
		BUILD_TYPE=TOOL
	else
		BUILD_TYPE=
	endif
else
	ifeq "$(LINUX)" "YES"
		ISYSROOT:=
		ifeq "$(shell (which clang >& /dev/null && which clang++ >& /dev/null && echo YES) || echo NO)" "YES"
			CC := clang
			CXX := clang++
		else
			CC := gcc
			CXX := g++
		endif
	endif
	export ICU_FOR_EMBEDDED_TRAINS:=NO
	TVOS:=0
	WATCHOS:=0
	BRIDGEOS:=0
	BUILD_TYPE=
endif

CROSS_BUILD:=$(ICU_FOR_EMBEDDED_TRAINS)
CROSSHOST_OBJROOT=$(OBJROOT)/crossbuildhost

# For Apple builds, set MAKEJOBS if undefined (as it normally will be).
# We could get a lot fancier here, for example see the makefile patch in rdar://28767058.
ifndef MAKEJOBS
	ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
	    export MAKEJOBS := -j6
	endif
endif


$(info # ICU_FOR_APPLE_PLATFORMS=$(ICU_FOR_APPLE_PLATFORMS))
$(info # HOSTCC=$(HOSTCC))
$(info # HOSTCXX=$(HOSTCXX))
$(info # HOSTISYSROOT=$(HOSTISYSROOT))
$(info # CC=$(CC))
$(info # CXX=$(CXX))
$(info # ISYSROOT=$(ISYSROOT))
$(info # ICU_FOR_EMBEDDED_TRAINS=$(ICU_FOR_EMBEDDED_TRAINS))
$(info # CROSS_BUILD=$(CROSS_BUILD))
$(info # BUILD_TYPE=$(BUILD_TYPE))

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

# Since we need to build a macOS version of ICU libraries and then a version of the data build
# tools that link against those libraries, all of those must be targeted for a macODS version
# that is not incompatible with the system we are running on. However we also want to make sure
# that if we are running on the latest development macOS with corresponding SDKs, that we do not
# build an ICU that cannot run on a recent released system. Thus we want to use the lowest version
# from among the following 3 versions:
# - MAC_OS_HOST_SYSVERSION, a string like "13.4" or "14.0": The system actually running
# - MAC_OS_HOST_SDKVERSION, a string like "13.4" or "14.0": The default macosx.internal SDK version
# - MAC_OS_HOST_MAXVERSION, a released version less than the current macOS release target,
#   to enable testing on a previous stable release. This should be updated every time a new
#   open-source ICU version is integrated to Apple ICU.Currently we set this to 14.5 = SunburstE.
#
# We split each of the three above version into their MAJOR and MINOR parts using cut (since makefile
# numeric comparison using "test x -lt Y" can only be done on integers). First we find the lower
# of MAC_OS_HOST_SYSVERSION and MAC_OS_HOST_SDKVERSION, with the result in MAC_OS_HOST_MAJOR1
# and MAC_OS_HOST_MINOR1. Then we find the lower of that and MAC_OS_HOST_MAXVERSION, with
# the result in MAC_OS_HOST_MAJOR2 and MAC_OS_HOST_MINOR2; we use that latter result to set
# MAC_OS_X_VERSION_MIN_REQUIRED and OSX_HOST_VERSION_MIN_STRING.
#
# Previously we just had:
# MAC_OS_X_VERSION_MIN_REQUIRED=130000
# OSX_HOST_VERSION_MIN_STRING=13.0.0

ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
  MAC_OS_HOST_SYSVERSION := $(shell sw_vers -productVersion)
  MAC_OS_HOST_SYSVERSION_MAJOR := $(shell echo $(MAC_OS_HOST_SYSVERSION) | cut -f1 -d.)
  MAC_OS_HOST_SYSVERSION_MINOR := $(shell echo $(MAC_OS_HOST_SYSVERSION) | cut -f2 -d.)

  MAC_OS_HOST_SDKVERSION := $(shell xcodebuild -version -sdk macosx.internal SDKVersion)
  MAC_OS_HOST_SDKVERSION_MAJOR := $(shell echo $(MAC_OS_HOST_SDKVERSION) | cut -f1 -d.)
  MAC_OS_HOST_SDKVERSION_MINOR := $(shell echo $(MAC_OS_HOST_SDKVERSION) | cut -f2 -d.)

  MAC_OS_HOST_MAXVERSION := 14.5
  MAC_OS_HOST_MAXVERSION_MAJOR := $(shell echo $(MAC_OS_HOST_MAXVERSION) | cut -f1 -d.)
  MAC_OS_HOST_MAXVERSION_MINOR := $(shell echo $(MAC_OS_HOST_MAXVERSION) | cut -f2 -d.)

  ifeq "$(shell test $(MAC_OS_HOST_SYSVERSION_MAJOR) -lt $(MAC_OS_HOST_SDKVERSION_MAJOR) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR1 = $(MAC_OS_HOST_SYSVERSION_MAJOR)
    MAC_OS_HOST_MINOR1 = $(MAC_OS_HOST_SYSVERSION_MINOR)
  else ifeq "$(shell test $(MAC_OS_HOST_SDKVERSION_MAJOR) -lt $(MAC_OS_HOST_SYSVERSION_MAJOR) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR1 = $(MAC_OS_HOST_SDKVERSION_MAJOR)
    MAC_OS_HOST_MINOR1 = $(MAC_OS_HOST_SDKVERSION_MINOR)
  else ifeq "$(shell test $(MAC_OS_HOST_SYSVERSION_MINOR) -lt $(MAC_OS_HOST_SDKVERSION_MINOR) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR1 = $(MAC_OS_HOST_SYSVERSION_MAJOR)
    MAC_OS_HOST_MINOR1 = $(MAC_OS_HOST_SYSVERSION_MINOR)
  else
    MAC_OS_HOST_MAJOR1 = $(MAC_OS_HOST_SDKVERSION_MAJOR)
    MAC_OS_HOST_MINOR1 = $(MAC_OS_HOST_SDKVERSION_MINOR)
  endif

  ifeq "$(shell test $(MAC_OS_HOST_MAXVERSION_MAJOR) -lt $(MAC_OS_HOST_MAJOR1) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR2 = $(MAC_OS_HOST_MAXVERSION_MAJOR)
    MAC_OS_HOST_MINOR2 = $(MAC_OS_HOST_MAXVERSION_MINOR)
  else ifeq "$(shell test $(MAC_OS_HOST_MAJOR1) -lt $(MAC_OS_HOST_MAXVERSION_MAJOR) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR2 = $(MAC_OS_HOST_MAJOR1)
    MAC_OS_HOST_MINOR2 = $(MAC_OS_HOST_MINOR1)
  else ifeq  "$(shell test $(MAC_OS_HOST_MAXVERSION_MINOR) -lt $(MAC_OS_HOST_MINOR1) && echo YES)" "YES"
    MAC_OS_HOST_MAJOR2 = $(MAC_OS_HOST_MAXVERSION_MAJOR)
    MAC_OS_HOST_MINOR2 = $(MAC_OS_HOST_MAXVERSION_MINOR)
  else
    MAC_OS_HOST_MAJOR2 = $(MAC_OS_HOST_MAJOR1)
    MAC_OS_HOST_MINOR2 = $(MAC_OS_HOST_MINOR1)
  endif

  MAC_OS_X_VERSION_MIN_REQUIRED := $(MAC_OS_HOST_MAJOR2)0$(MAC_OS_HOST_MINOR2)00
  OSX_HOST_VERSION_MIN_STRING :=  $(MAC_OS_HOST_MAJOR2).$(MAC_OS_HOST_MINOR2).0

  $(info # MAC_OS_HOST_SYSVERSION=$(MAC_OS_HOST_SYSVERSION))
  $(info # MAC_OS_HOST_SDKVERSION=$(MAC_OS_HOST_SDKVERSION))
  $(info # MAC_OS_HOST_MAXVERSION=$(MAC_OS_HOST_MAXVERSION))
  $(info # MAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED))
  $(info # OSX_HOST_VERSION_MIN_STRING=$(OSX_HOST_VERSION_MIN_STRING))
else
  MAC_OS_X_VERSION_MIN_REQUIRED:=0
  OSX_HOST_VERSION_MIN_STRING:=0
endif

ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
    export TOOLSEXTRAFLAGS:=
else ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
    export TOOLSEXTRAFLAGS:=-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING)
else
    export TOOLSEXTRAFLAGS:=
endif
$(info # TOOLSEXTRAFLAGS=$(TOOLSEXTRAFLAGS))

ifneq "$(RC_PLATFORM_NAME)" ""
 export SDK_PLATFORM_NORMALIZED := $(shell echo $(RC_PLATFORM_NAME) | tr '[:upper:]' '[:lower:]')
else ifneq "$(PLATFORMROOT)" ""
 export SDK_PLATFORM_NORMALIZED := $(shell basename "$(PLATFORMROOT)" | tr '[:upper:]' '[:lower:]' | sed 's/\.platform$$//')
else ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
 export SDK_PLATFORM_NORMALIZED := macosx
else
 export SDK_PLATFORM_NORMALIZED :=
endif
$(info # SDK_PLATFORM_NORMALIZED=$(SDK_PLATFORM_NORMALIZED))

ifeq "$(SDKVERSION)" ""
  export SDKVERSION := $(shell xcodebuild -version -sdk $(SDK_PLATFORM_NORMALIZED) SDKVersion | head -1)
endif
$(info # SDKVERSION=$(SDKVERSION))

ifeq "$(SDK_PLATFORM_NORMALIZED)" "iphoneos"
  TARGET_TRIPLE_SYS = ios
else ifeq "$(SDK_PLATFORM_NORMALIZED)" "macosx"
  TARGET_TRIPLE_SYS = macos
else ifeq "$(SDK_PLATFORM_NORMALIZED)" "appletvos"
  TARGET_TRIPLE_SYS = tvos
else
  TARGET_TRIPLE_SYS = $(SDK_PLATFORM_NORMALIZED)
endif

# arch can be left out because it's explicitly passed in as -arch
ifeq "$(BUILD_TYPE)" "DEVICE"
  ICU_TARGET_VERSION := -target unknown-apple-$(TARGET_TRIPLE_SYS)$(SDKVERSION)
else ifeq "$(BUILD_TYPE)" "SIMULATOR"
  ICU_TARGET_VERSION := -target unknown-apple-$(TARGET_TRIPLE_SYS)$(SDKVERSION)-simulator
endif

$(info # ICU_TARGET_VERSION=$(ICU_TARGET_VERSION))


DISABLE_DRAFT:=$(ICU_FOR_EMBEDDED_TRAINS)
ifeq "$(DISABLE_DRAFT)" "YES"
	DRAFT_FLAG=--disable-draft
else
	DRAFT_FLAG=
endif

# For some reason, under cygwin, bash uname is not found, and
# sh uname does not produce a result with -p or -m. So we just
# hardcode here.
ifeq "$(WINDOWS)" "YES"
	UNAME_PROCESSOR:=i386
else
	UNAME_PROCESSOR:=$(shell uname -p)
endif

ifneq "$(RC_ARCHS)" ""
	INSTALLHDRS_ARCH=-arch $(shell echo $(RC_ARCHS) | cut -d' ' -f1)
else
	INSTALLHDRS_ARCH=
endif
$(info # INSTALLHDRS_ARCH=$(INSTALLHDRS_ARCH))
$(info # buildhost=$(UNAME_PROCESSOR))


# FORCEENDIAN below is to override silly configure behavior in which if
# __APPLE_CC__ is defined and archs are in { ppc, ppc64, i386, x86_64 }
# then it assumes a universal build (ac_cv_c_bigendian=universal) with
# data file initially built big-endian.
#
# darwin releases
# darwin 18.0.0 2018-Sep: macOS 10.14, iOS 12
# darwin 18.2.0 2018-Oct: macOS 10.14.1, iOS 12.1
# darwin 19.0.0 2019-Oct: macOS 10.15, iOS 13
# darwin 20.0.0 2020-fall: macOS 11, iOS 14
# darwin 21.0.0 2021-fall: macOS 12, iOS 15
#
ifeq "$(CROSS_BUILD)" "YES"
	RC_ARCHS_FIRST=$(shell echo $(RC_ARCHS) | cut -d' ' -f1)
	TARGET_SPEC=$(RC_ARCHS_FIRST)-apple-darwin21.0.0
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
	TARGET_SPEC=$(UNAME_PROCESSOR)-apple-darwin21.0.0
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

# even for a crossbuild host build, we want to use the target's latest tzdata as pointed to by latest_tzdata.tar.gz;
# first try RC_EMBEDDEDPROJECT_DIR (<rdar://problem/28141177>), else SDKPATH.
# Note: local buildit builds should specify -nolinkEmbeddedProjects so we use SDKPATH, since RC_EMBEDDEDPROJECT_DIR
# is not meaningful (alternatively, RC_EMBEDDEDPROJECT_DIR can be set to to point to a location with a subdirectory
# TimeZoneData containing time zone data as in a particular version of the TimeZoneData project).
ifdef RC_EMBEDDEDPROJECT_DIR
	ifeq "$(shell test -L $(RC_EMBEDDEDPROJECT_DIR)/TimeZoneData/usr/local/share/tz/latest_tzdata.tar.gz && echo YES )" "YES"
		export TZDATA:=$(RC_EMBEDDEDPROJECT_DIR)/TimeZoneData/usr/local/share/tz/$(shell readlink $(RC_EMBEDDEDPROJECT_DIR)/TimeZoneData/usr/local/share/tz/latest_tzdata.tar.gz)
	endif
	ifeq "$(shell test -d $(RC_EMBEDDEDPROJECT_DIR)/TimeZoneData/usr/local/share/tz/icudata && echo YES )" "YES"
		export TZAUXFILESDIR:=$(RC_EMBEDDEDPROJECT_DIR)/TimeZoneData/usr/local/share/tz/icudata
	endif
endif
ifndef TZDATA
	ifeq "$(shell test -L $(SDKPATH)/usr/local/share/tz/latest_tzdata.tar.gz && echo YES )" "YES"
		export TZDATA:=$(SDKPATH)/usr/local/share/tz/$(shell readlink $(SDKPATH)/usr/local/share/tz/latest_tzdata.tar.gz)
	endif
	ifeq "$(shell test -d $(SDKPATH)/usr/local/share/tz/icudata && echo YES )" "YES"
		export TZAUXFILESDIR:=$(SDKPATH)/usr/local/share/tz/icudata
	endif
endif
$(info # RC_EMBEDDEDPROJECT_DIR=$(RC_EMBEDDEDPROJECT_DIR))
$(info # TZDATA=$(TZDATA))
$(info # TZAUXFILESDIR=$(TZAUXFILESDIR))
ifndef TZAUXFILESDIR
	TZAUXFILESDIR:=.
endif

DSYMTOOL := /usr/bin/dsymutil
DSYMSUFFIX := .dSYM

#################################
# Headers
#################################

# For installhdrs. Not every compiled module has an associated header. Normally,
# ICU installs headers as a sub-target of the install target. But since we only want
# certain libraries to install (and since we link all of our own .o modules), we need
# invoke the headers targets ourselves. This may be problematic because there isn't a
# good way to dist-clean afterwards...we need to do explicit dist-cleans, especially if
# install the extra libraries.

EXTRA_HDRS =
# EXTRA_HDRS = ./extra/ustdio/ ./layout/
ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else ifeq "$(WINDOWS)" "YES"
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ $(EXTRA_HDRS)
else
    HDR_MAKE_SUBDIR = ./common/ ./i18n/ ./io/ $(EXTRA_HDRS)
endif
ifeq "$(WINDOWS)" "YES"
	PRIVATE_HDR_PREFIX=/AppleInternal
else ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
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
ifeq "$(LINUX)" "YES"
STUB_DATA_OBJ = ./data/out/tmp/*.o
else
STUB_DATA_OBJ = ./stubdata/*.o
endif
EXTRA_LIBS =
#EXTRA_LIBS =./extra/ ./layout/ ./tools/ctestfw/ ./tools/toolutil/
#DATA_OBJ = ./data/out/build/*.o
ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
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

INSTALLSRC_VARFILES=./ICU_embedded.order \
	./apple/minimalapis.txt ./apple/minimalapisTest.c \
	./apple/BuildICUForAAS_script.bat ./apple/EXPORT.APPLE

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
			$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
	else
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples --disable-icuio \
			--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=32 \
			$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
	endif
else ifeq "$(LINUX)" "YES"
	ifeq "$(ARCH64)" "YES"
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
			--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=64 \
			$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
	else
		CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
			--with-data-packaging=library --prefix=$(PRIVATE_HDR_PREFIX) --with-library-bits=32 \
			$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
	endif
else ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
		--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
else
	CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
		--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX) \
		$(DRAFT_FLAG) $(OTHER_CONFIG_FLAGS)
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
ICU_VERS = 74
ICU_SUBVERS = 2
CORE_VERS = A

ifeq "$(WINDOWS)" "YES"
	DYLIB_SUFF = dll
	INSTALL_BASE_DIR ?= /usr/
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
	INSTALL_BASE_DIR ?= '/usr/local/'
	ifeq "$(ARCH64)" "YES"
		libdir = $(INSTALL_BASE_DIR)lib/
	else
		libdir = $(INSTALL_BASE_DIR)lib32/
	endif
	winprogdir =
	winintlibdir =
else
	DYLIB_SUFF = dylib
	INSTALL_BASE_DIR ?= /usr/
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
# InstallAPI -> libicucore
#################################

# Allow B&I to override installAPI when necessary. 
# InstallAPI is only supported when building for Darwin Platforms.

ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
ifeq ($(RC_ProjectName), $(filter $(RC_ProjectName),ICU ICU_Sim))
  SUPPORTS_TEXT_BASED_API ?= YES
  $(info # SUPPORTS_TEXT_BASED_API=$(SUPPORTS_TEXT_BASED_API))
endif # RC_ProjectName 
endif # ICU_FOR_APPLE_PLATFORMS

ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
  # Create complete target triples for tapi and pass them all at once. 
  ifeq "$(BUILD_TYPE)" "DEVICE"
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${SDKVERSION}, ${RC_ARCHS})
  else ifeq "$(BUILD_TYPE)" "SIMULATOR"
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${SDKVERSION}-simulator, ${RC_ARCHS})
  else # macOS case
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${SDKVERSION}, ${RC_ARCHS})
  	TARGET_TRIPLES :=$(TARGET_TRIPLES) $(patsubst %, -target-variant ${ARCH}%-apple-ios-macabi, ${RC_ARCHS})
  endif
  
  
  ICU_TBD_PATH = $(OBJROOT_CURRENT)/usr/lib/lib$(LIB_NAME).tbd
  TAPI_COMMON_OPTS := -dynamiclib \
  	-xc++ -std=c++11 \
  	$(TARGET_TRIPLES) \
  	-fvisibility=hidden \
  	-isysroot $(SDKROOT) \
  	-iquote $(SRCROOT)/icu/icu4c/source \
  	-iquote $(SRCROOT)/icu/icu4c/source/common \
  	-iquote $(SRCROOT)/icu/icu4c/source/i18n \
  	-install_name $(libdir)$($(INSTALLED_DYLIB_icu)) \
  	-current_version $(ICU_VERS).$(ICU_SUBVERS) \
    -compatibility_version 1 \
  	-DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" \
  	-DU_SHOW_INTERNAL_API=1 \
  	-DU_DISABLE_RENAMING=1 \
  	-DU_SHOW_CPLUSPLUS_API=1 \
  	-DU_DEFAULT_SHOW_DRAFT=0 \
  	-DU_SHOW_DRAFT_API \
  	-DU_HAVE_STRTOD_L=1 \
  	-DU_HAVE_XLOCALE_H=1 \
  	-DU_HAVE_STRING_VIEW=1 \
  	-DU_COMBINED_IMPLEMENTATION=1 \
  	-DU_LOCAL_SERVICE_HOOK=1 \
  	-DU_TIMEZONE=timezone \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dtitv_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dt_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dtptngen_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/selfmtimpl.h \
    -o $(ICU_TBD_PATH) \
  	-filelist $(OBJROOT_CURRENT)/tapi_headers.json
  
  TAPI_VERIFY_OPTS := $(TAPI_COMMON_OPTS) \
              --verify-mode=Pedantic \
              --verify-against=$(OBJROOT_CURRENT)/$(DYLIB)
endif # SUPPORTS_TEXT_BASED_API

#################################
# Data files
#################################

OPEN_SOURCE_VERSIONS_DIR=/usr/local/OpenSourceVersions/
OPEN_SOURCE_LICENSES_DIR=/usr/local/OpenSourceLicenses/

B_DATA_FILE=icudt$(ICU_VERS)b.dat
L_DATA_FILE=icudt$(ICU_VERS)l.dat
DATA_BUILD_SUBDIR= data/out
DATA_INSTALL_DIR=$(INSTALL_BASE_DIR)share/icu/

# DATA_LOOKUP_DIR is what the target ICU_DATA_DIR gets set to in CFLAGS, CXXFLAGS;
# DATA_LOOKUP_DIR_BUILDHOST is what any crossbuild host ICU_DATA_DIR gets set to.
# Formerly we had DATA_LOOKUP_DIR=/var/db/icu/ for embedded non-simulator builds
# and DATA_LOOKUP_DIR=/usr/share/icu/ for everything else. Now all systems look
# in the same place for the main data file:
DATA_LOOKUP_DIR=$(DATA_INSTALL_DIR)
DATA_LOOKUP_DIR_BUILDHOST=$(DATA_INSTALL_DIR)

# Timezone data file(s)
# ICU will look for /var/db/timezone/icutz/icutz44l.dat
# If directory or file is not present, the timesone data in
# current data file e.g. /usr/share/icu/icudt56l.dat will be used.
# Currently we are not conditionalizing the definition of
# TZDATA_LOOKUP_DIR as in
#	ifeq "$(BUILD_TYPE)" "DEVICE"
#		TZDATA_LOOKUP_DIR = /var/db/timezone/icutz
#	else
#	...
# since the code stats the path for TZDATA_LOOKUP_DIR and does
# not try to use it if it does not exist. We could define it
# as TZDATA_LOOKUP_DIR = /usr/share/icutz when not needed...
# TZDATA_LOOKUP_DIR is passed to compiler as U_TIMEZONE_FILES_DIR
# TZDATA_PACKAGE is passed to compiler as U_TIMEZONE_PACKAGE
TZDATA_LOOKUP_DIR = /var/db/timezone/icutz
TZDATA_PACKAGE = icutz44l
TZDATA_FORMAT_STRING = "44l"
TZDATA_FORMAT_FILE = icutzformat.txt


# Name of runtime environment variable to get the prefix path for DATA_LOOKUP_DIR
# Currently we are only using this for LINUX, should also use for iOS simulator
ifeq "$(LINUX)" "YES"
	DATA_DIR_PREFIX_ENV_VAR=APPLE_FRAMEWORKS_ROOT
else
	DATA_DIR_PREFIX_ENV_VAR=
endif

#################################
# Tools
#################################

localtooldir=/usr/local/bin/
localtztooldir=/usr/local/bin/tztools/
locallibdir=/usr/local/lib/

INFOTOOL = icuinfo
INFOTOOL_OBJS = ./tools/icuinfo/icuinfo.o ./tools/toolutil/udbgutil.o ./tools/toolutil/uoptions.o

ICUZDUMPTOOL = icuzdump
ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
    ICUZDUMPTOOL_OBJS = ./tools/tzcode/icuzdump.o $(IO_OBJ)
else
    ICUZDUMPTOOL_OBJS = ./tools/tzcode/icuzdump.o
endif

TOOLSLIB_NAME = icutu
TOOLS_DYLIB = libicutu.$(DYLIB_SUFF)
TOOLS_DYLIB_OBJS = ./tools/toolutil/*.o

# The following modified version enables the tz toools to be used on systems with ICU 55 or later.
# It is used with the toolchain tools below.
TOOLSLIB_NAME_FORTOOLS = icutux
TOOLS_DYLIB_FORTOOLS = libicutux.$(DYLIB_SUFF)
TOOLS_DYLIB_OBJS_FORTOOLS = ./tools/toolutil/collationinfo.o ./tools/toolutil/filestrm.o \
./tools/toolutil/package.o ./tools/toolutil/pkg_icu.o ./tools/toolutil/pkgitems.o \
./tools/toolutil/swapimpl.o ./tools/toolutil/toolutil.o ./tools/toolutil/ucbuf.o \
./tools/toolutil/unewdata.o ./tools/toolutil/uoptions.o ./tools/toolutil/uparse.o ./tools/toolutil/writesrc.o \
$(COMMON_OBJ) $(STUB_DATA_OBJ)

ZICTOOL = icuzic
ZICTOOL_OBJS = ./tools/tzcode/zic.o ./tools/tzcode/localtime.o ./tools/tzcode/asctime.o ./tools/tzcode/scheck.o ./tools/tzcode/ialloc.o

RESTOOL = icugenrb
RESTOOL_OBJS = ./tools/genrb/errmsg.o ./tools/genrb/genrb.o ./tools/genrb/parse.o ./tools/genrb/read.o ./tools/genrb/reslist.o ./tools/genrb/ustr.o \
				./tools/genrb/rbutil.o ./tools/genrb/wrtjava.o ./tools/genrb/rle.o ./tools/genrb/wrtxml.o ./tools/genrb/prscmnts.o ./tools/genrb/filterrb.o

# note there is also a symbol ICUPKGTOOL which refers to the one used
# internally which is linked against the separate uc and i18n libs.
PKGTOOL = icupkg
PKGTOOL_OBJS = ./tools/icupkg/icupkg.o

TZ2ICUTOOL = tz2icu
TZ2ICUTOOL_OBJS = ./tools/tzcode/tz2icu.o

GENBRKTOOL = icugenbrk
GENBRKTOOL_OBJS = ./tools/genbrk/genbrk.o

#################################
# Ancillary files
# e.g. supplementalData.xml, per <rdar://problem/13426014>
# These are like internal headers in that they are only installed for
# (other) projects to build against, not needed at runtime.
# Install during installhdrs? That does not seem to work for CLDRFILESDIR
#################################

CLDRFILESDIR=/usr/local/share/cldr

EMOJI_DATA_DIR=/usr/local/share/emojiData

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

CFLAGS_SANITIZER :=
CXXFLAGS_SANITIZER :=
LDFLAGS_SANITIZER :=
ifeq ($(RC_ENABLE_ADDRESS_SANITIZATION),1)
  $(info Enabling Address Sanitizer)
  ASAN_FLAGS:=-fsanitize=address
  CFLAGS_SANITIZER += $(ASAN_FLAGS)
  CXXFLAGS_SANITIZER += $(ASAN_FLAGS)
  LDFLAGS_SANITIZER += $(ASAN_FLAGS)
endif

ifeq ($(RC_ENABLE_UNDEFINED_BEHAVIOR_SANITIZATION),1)
  $(info Enabling Undefined Behaviour Sanitizer)
  UBSAN_FLAGS:=-fsanitize=undefined
  CFLAGS_SANITIZER += $(UBSAN_FLAGS)
  CXXFLAGS_SANITIZER += $(UBSAN_FLAGS)
  LDFLAGS_SANITIZER += $(UBSAN_FLAGS)
endif

ifeq ($(RC_ENABLE_THREAD_SANITIZATION),1)
  $(info Enabling Thread Sanitizer)
  TSAN_FLAGS:=-fsanitize=thread
  CFLAGS_SANITIZER += $(TSAN_FLAGS)
  CXXFLAGS_SANITIZER += $(TSAN_FLAGS)
  LDFLAGS_SANITIZER += $(TSAN_FLAGS)
endif
LDFLAGS += $(LDFLAGS_SANITIZER)

APPLE_HARDENING_OPTS := -D_FORTIFY_SOURCE=2 -fstack-protector-strong
APPLE_STACK_INIT_OPTS := -ftrivial-auto-var-init=zero

# For normal Windows builds set the ENV= options here; for debug Windows builds set the ENV_DEBUG=
# options here and also the update the LINK.EXE lines in the TARGETS section below.
ifeq "$(WINDOWS)" "YES"
	ifeq "$(ARCH64)" "YES"
		ENV= CFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
			CXXFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t /DU_SHOW_CPLUSPLUS_API=1" \
			LDFLAGS="/NXCOMPAT /DYNAMICBASE /DEBUG /OPT:REF"
	else
		ENV= CFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
			CXXFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /Zi /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t /DU_SHOW_CPLUSPLUS_API=1" \
			LDFLAGS="/NXCOMPAT /SAFESEH /DYNAMICBASE /DEBUG /OPT:REF"
	endif
	ENV_CONFIGURE= CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)" \
		CFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
		CXXFLAGS="/utf-8 /O2 /Ob2 /MD /GF /GS /nologo /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /Zc:wchar_t /DU_SHOW_CPLUSPLUS_API=1"
	ENV_DEBUG= CFLAGS="/utf-8 /O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES" \
		CXXFLAGS="/utf-8 /O2 /Ob2 /MDd /GF /GS /Zi /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES /EHsc /DU_SHOW_CPLUSPLUS_API=1" \
		LDFLAGS="/DEBUG /DYNAMICBASE"
	ENV_PROFILE=
else ifeq "$(LINUX)" "YES"
	ifeq "$(ARCH64)" "YES"
		ENV_CONFIGURE= \
			LANG="en_US.utf8" \
			CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)"  \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV_DEBUG= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV_PROFILE= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m64 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"
	else
		ENV_CONFIGURE= \
			LANG="en_US.utf8" \
			CPPFLAGS="-DU_DISABLE_RENAMING=1 $(DEFINE_BUILD_LEVEL)" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV_DEBUG= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -O0 -gfull -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

		ENV_PROFILE= \
			LANG="en_US.utf8" \
			CC="$(CC)" \
			CXX="$(CXX)" \
			CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden" \
			CXXFLAGS="-std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DICU_DATA_DIR_PREFIX_ENV_VAR=\"\\\"$(DATA_DIR_PREFIX_ENV_VAR)\\\"\" -m32 -g -Os -pg -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
			TZDATA="$(TZDATA)" \
			DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"
	endif
	LDFLAGS += -lpthread
else
	CPPOPTIONS =
	ENV_CONFIGURE= \
		CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) $(ISYSROOT) $(ENV_CONFIGURE_ARCHS)" \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(APPLE_HARDENING_OPTS) $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" $(FORCEENDIAN)\
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

	ENV= \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(APPLE_HARDENING_OPTS) $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

	ENV_DEBUG= \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(APPLE_HARDENING_OPTS) $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

	ENV_PROFILE= \
		CC="$(CC)" \
		CXX="$(CXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(APPLE_HARDENING_OPTS) $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -pg -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(THUMB_FLAG) $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		RC_ARCHS="$(RC_ARCHS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

	ENV_CONFIGURE_BUILDHOST= \
		CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) $(HOSTISYSROOT)" \
		CC="$(HOSTCC)" \
		CXX="$(HOSTCXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

	ENV_BUILDHOST= \
		CC="$(HOSTCC)" \
		CXX="$(HOSTCXX)" \
		CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(CFLAGS_SANITIZER)" \
		CXXFLAGS="--std=c++11 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR_BUILDHOST)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(CXXFLAGS_SANITIZER)" \
		TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
		TZDATA="$(TZDATA)" \
		DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

endif

ENV_icu = ENV
ENV_debug = ENV_DEBUG
ENV_profile = ENV_PROFILE

ifeq "$(BUILD_TYPE)" "DEVICE"
	ORDERFILE=$(SDKPATH)/AppleInternal/OrderFiles/libicucore.order
else ifeq "$(BUILD_TYPE)" "SIMULATOR"
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

.PHONY : icu check installsrc installhdrs installhdrsint installapi clean install installapi-verify debug \
	debug-install crossbuildhost icutztoolsforsdk icudata
.DELETE_ON_ERROR :

# Rule for adjusting sources for different train types.
# Assumes current directory is icu/icu4c/source to be patched.
# This may be:
#   $(SRCROOT)/icu/icu4c/source for installsrc, or
#   $(OBJROOT_CURRENT) if sources are copied for e.g. a local make.
#
# The various patchconfig files should assume the current directory is icu/icu4c/source.
#
# Note that if sources have been installed by installsrc (only run as part of buildit or
# submitproject for Apple platforms, not for Windows/Linux), then
#    $(SRCROOT)/installsrcNotRunFlag is not present, and
#    ADJUST_SOURCES has already been run.
# Otherwise, if we are doing a local build (e.g. make check, make install), or if
# sources were submitted using submitproject for non-Apple platforms (Windows/Linux) then
#    $(SRCROOT)/installsrcNotRunFlag is present, and
#    ADJUST_SOURCES has not been run (run it after copying sources to OBJROOT_CURRENT)
#

ADJUST_SOURCES = \
		if test "$(WINDOWS)" = "YES"; then \
			mv data/unidata/base_unidata/*.txt data/unidata/; \
			mv data/unidata/norm2/base_norm2/*.txt data/unidata/norm2/; \
			mv data/in/base_in/*.nrm data/in/; \
			mv data/in/base_in/*.icu data/in/; \
		fi


icu debug profile : $(OBJROOT_CURRENT)/Makefile
	echo "# start make for target"
	date "+# %F %T %z"
	(cd $(OBJROOT_CURRENT); \
		$(MAKE) $(MAKEJOBS) $($(ENV_$@)) || exit 1; \
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
				if test ! "$(ICU_FOR_EMBEDDED_TRAINS)" = "YES"; then \
					ZIPPERING_LDFLAGS=-Wl,-iosmac_version_min,12.0; \
				fi; \
				$($(ENV_$@)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
					$(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG) \
					$(CXXFLAGS) $(LDFLAGS) $$ZIPPERING_LDFLAGS -single_module $(SECTORDER_FLAGS) -dead_strip \
					-install_name $(libdir)$($(INSTALLED_DYLIB_$@)) -o ./$($(INSTALLED_DYLIB_$@)) $(DYLIB_OBJS); \
				if test "$@" = "icu"; then \
					ln -fs  $(INSTALLED_DYLIB) $(DYLIB); \
					echo '# build' $(INFOTOOL) 'linked against' $(LIB_NAME) ; \
					$($(ENV_$@)) $(CXX) --std=c++11 $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG) \
						$(LDFLAGS) -dead_strip -o ./$(INFOTOOL) $(INFOTOOL_OBJS) -L./ -l$(LIB_NAME) ; \
					echo '# build' $(TOOLS_DYLIB) 'linked against' $(LIB_NAME) ; \
					$($(ENV_$@)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
						$(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) $(THUMB_FLAG) \
						$(CXXFLAGS) $(LDFLAGS) -single_module \
						-install_name $(locallibdir)$(TOOLS_DYLIB) -o ./$(TOOLS_DYLIB) $(TOOLS_DYLIB_OBJS) -L./ -l$(LIB_NAME) ; \
					echo '# build' $(ICUZDUMPTOOL) 'linked against' $(TOOLSLIB_NAME) $(LIB_NAME) ; \
					$($(ENV_$@)) $(LINK.cc) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) $(THUMB_FLAG) --std=c++11 \
						-dead_strip -o ./$(ICUZDUMPTOOL) $(ICUZDUMPTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME) -l$(LIB_NAME); \
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
			printf $(TZDATA_FORMAT_STRING) > $(TZDATA_FORMAT_FILE); \
		fi; \
	);
	echo "# end make for target"
	date "+# %F %T %z"

crossbuildhost : $(CROSSHOST_OBJROOT)/Makefile
	echo "# start make for crossbuild host"
	date "+# %F %T %z"
	(cd $(CROSSHOST_OBJROOT); \
		$(MAKE) $(MAKEJOBS) $($(ENV_BUILDHOST)) || exit 1; \
	);
	echo "# end make for crossbuild host"
	date "+# %F %T %z"

# For the install-icutztoolsforsdk target, SDKROOT will always be an OSX SDK root.
ifeq "$(SDK_PLATFORM_NORMALIZED)" "macosx"
icutztoolsforsdk : $(OBJROOT_CURRENT)/Makefile
	echo "# start make icutztoolsforsdk"
	echo "SDK_PLATFORM_NORMALIZED=$(SDK_PLATFORM_NORMALIZED)"
	date "+# %F %T %z"
	(cd $(OBJROOT_CURRENT); \
		$(MAKE) $(MAKEJOBS) $(ENV) || exit 1; \
		echo '# build' $(TOOLS_DYLIB_FORTOOLS) 'linked against' $(LIB_NAME) ; \
		$($(ENV)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
			-g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) \
			$(CXXFLAGS) $(LDFLAGS) -single_module \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			-install_name $(locallibdir)$(TOOLS_DYLIB_FORTOOLS) -o ./$(TOOLS_DYLIB_FORTOOLS) $(TOOLS_DYLIB_OBJS_FORTOOLS) -L./ -l$(LIB_NAME) || exit 1 ; \
		echo '# build' $(ZICTOOL) 'linked against' $(TOOLSLIB_NAME_FORTOOLS) ; \
		$($(ENV_BUILDHOST)) $(CXX) --std=c++11 -g -Os -isysroot $(HOSTSDKPATH) \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			$(LDFLAGS) -dead_strip -o ./$(ZICTOOL) $(ZICTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME_FORTOOLS) || exit 1 ; \
		echo '# build' $(RESTOOL) 'linked against' $(TOOLSLIB_NAME_FORTOOLS) $(LIB_NAME) ; \
		$($(ENV_BUILDHOST)) $(CXX) --std=c++11 -g -Os -isysroot $(HOSTSDKPATH) \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			$(LDFLAGS) -dead_strip -o ./$(RESTOOL) $(RESTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME_FORTOOLS) -l$(LIB_NAME) || exit 1 ; \
		echo '# build' $(PKGTOOL) 'linked against' $(TOOLSLIB_NAME_FORTOOLS) ; \
		$($(ENV_BUILDHOST)) $(CXX) --std=c++11 -g -Os -isysroot $(HOSTSDKPATH) \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			$(LDFLAGS) -dead_strip -o ./$(PKGTOOL) $(PKGTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME_FORTOOLS) || exit 1 ; \
		echo '# build' $(TZ2ICUTOOL) 'linked against' $(TOOLSLIB_NAME_FORTOOLS) ; \
		$($(ENV_BUILDHOST)) $(CXX) --std=c++11 -g -Os -isysroot $(HOSTSDKPATH) \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			$(LDFLAGS) -dead_strip -o ./$(TZ2ICUTOOL) $(TZ2ICUTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME_FORTOOLS) || exit 1 ; \
		echo '# build' $(GENBRKTOOL) 'linked against' $(TOOLSLIB_NAME_FORTOOLS) ; \
		$($(ENV_BUILDHOST)) $(CXX) --std=c++11 -g -Os -isysroot $(HOSTSDKPATH) \
			-mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING) $(RC_ARCHS:%=-arch %) \
			$(LDFLAGS) -dead_strip -o ./$(GENBRKTOOL) $(GENBRKTOOL_OBJS) -L./ -l$(TOOLSLIB_NAME_FORTOOLS) || exit 1 ; \
	);
	echo "# end make icutztoolsforsdk"
	date "+# %F %T %z"
else
icutztoolsforsdk :
	$(info icutztoolsforsdk not supported for embedded platforms)
endif

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

fuzzer: icu
	(cd $(OBJROOT_CURRENT)/test/fuzzer; \
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
	cp -Rpf $(SRCROOT)/icu/icu4c/source/* $(OBJROOT_CURRENT)/;
	(cd $(OBJROOT_CURRENT); \
		if test -e $(SRCROOT)/installsrcNotRunFlag ; then $(ADJUST_SOURCES); fi; \
		if test -f $(TZAUXFILESDIR)/metaZones.txt ; then cp -p $(TZAUXFILESDIR)/metaZones.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/timezoneTypes.txt ; then cp -p $(TZAUXFILESDIR)/timezoneTypes.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/windowsZones.txt ; then cp -p $(TZAUXFILESDIR)/windowsZones.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/icuregions ; then cp -p $(TZAUXFILESDIR)/icuregions tools/tzcode/; fi; \
		if test -f $(TZAUXFILESDIR)/icuzones ; then cp -p $(TZAUXFILESDIR)/icuzones tools/tzcode/; fi; \
		if test "$(WINDOWS)" = "YES"; then \
			echo "# configure for target"; \
			$(ENV_CONFIGURE) ./runConfigureICU Cygwin/MSVC $(CONFIG_FLAGS); \
		elif test "$(LINUX)" = "YES"; then \
			echo "# configure for target"; \
			$(ENV_CONFIGURE) ./runConfigureICU Linux $(CONFIG_FLAGS); \
		elif test "$(CROSS_BUILD)" = "YES"; then \
			echo "# configure for crossbuild target"; \
			$(ENV_CONFIGURE) ./configure --host=$(TARGET_SPEC) --with-cross-build=$(CROSSHOST_OBJROOT) $(CONFIG_FLAGS); \
		else \
			echo "# configure for non-crossbuild target"; \
			$(ENV_CONFIGURE) ./runConfigureICU MacOSX $(CONFIG_FLAGS); \
		fi; \
	);

$(CROSSHOST_OBJROOT)/Makefile :
	if test ! -d $(CROSSHOST_OBJROOT); then \
		mkdir -p $(CROSSHOST_OBJROOT); \
	fi;
	cp -Rpf $(SRCROOT)/icu/icu4c/source/* $(CROSSHOST_OBJROOT);
	(cd $(CROSSHOST_OBJROOT); \
		if test -e $(SRCROOT)/installsrcNotRunFlag; then $(ADJUST_SOURCES); fi; \
		if test -f $(TZAUXFILESDIR)/metaZones.txt ; then cp -p $(TZAUXFILESDIR)/metaZones.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/timezoneTypes.txt ; then cp -p $(TZAUXFILESDIR)/timezoneTypes.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/windowsZones.txt ; then cp -p $(TZAUXFILESDIR)/windowsZones.txt data/misc/; fi; \
		if test -f $(TZAUXFILESDIR)/icuregions ; then cp -p $(TZAUXFILESDIR)/icuregions tools/tzcode/; fi; \
		if test -f $(TZAUXFILESDIR)/icuzones ; then cp -p $(TZAUXFILESDIR)/icuzones tools/tzcode/; fi; \
		echo "# configure for crossbuild host"; \
		$(ENV_CONFIGURE_BUILDHOST) ./runConfigureICU MacOSX $(CONFIG_FLAGS); \
	);

#################################
# B&I TARGETS
#################################

# Since our sources are in icu/icu4c/source (ignore the ICU subdirectory for now), we wish to
# copy them to somewhere else. We tar it to stdout, cd to the appropriate directory, and
# untar from stdin.
# Do NOT include installsrcNotRunFlag in the list of files to tar up, that defeats the purpose.

installsrc :
	if test ! -d $(SRCROOT); then mkdir $(SRCROOT); fi;
	if test -d $(SRCROOT)/icu/icu4c/source ; then rm -rf $(SRCROOT)/icu/icu4c/source; fi;
	tar cf - ./makefile ./ICU.plist ./icu/LICENSE ./icu/icu4c/source ./apple/cldrFiles ./apple/emojiData ./apple/modules \
		./apple/generate_json_for_tapi.py $(INSTALLSRC_VARFILES) | (cd $(SRCROOT) ; tar xfp -); \
	(cd $(SRCROOT)/icu/icu4c/source; $(ADJUST_SOURCES) );

# This works. Just not for ~ in the DSTROOT. We run configure first (in case it hasn't
# been already). Then we make the install-headers target on specific makefiles (since
# not every subdirectory/sub-component has a install-headers target).

# installhdrs should be no-op for BUILD_TYPE=TOOL
ifeq "$(BUILD_TYPE)" "TOOL"
installhdrs :
else
installhdrs : installhdrsint
endif

MKINSTALLDIRS=$(SHELL) $(SRCROOT)/icu/icu4c/source/mkinstalldirs
INSTALL_DATA=${INSTALL} -m 644
UNIFDEF=unifdef
ifeq "$(ICU_FOR_EMBEDDED_TRAINS)" "YES"
UNIFDEF_FLAGS=-DBUILD_FOR_EMBEDDED -UBUILD_FOR_MACOS
else ifeq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
UNIFDEF_FLAGS=-DBUILD_FOR_MACOS -UBUILD_FOR_EMBEDDED
endif

ifneq "$(ICU_FOR_APPLE_PLATFORMS)" "YES"
installhdrsint : $(OBJROOT_CURRENT)/Makefile
else ifneq "$(RC_XBS)" "YES"
installhdrsint : $(OBJROOT_CURRENT)/Makefile
else
installhdrsint :
endif
	if test ! -d $(OBJROOT_CURRENT); then \
		mkdir -p $(OBJROOT_CURRENT); \
	fi;
	(if test -e $(SRCROOT)/installsrcNotRunFlag; then cd $(OBJROOT_CURRENT); else cd $(SRCROOT)/icu/icu4c/source/; fi; \
		$(MKINSTALLDIRS) $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/; \
		for subdir in $(HDR_MAKE_SUBDIR); do \
			echo "# Subdir $$subdir"; \
			(cd $$subdir/unicode; \
				for i in *.h; do \
	 				echo "$(INSTALL_DATA) $$i $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/"; \
	 				$(INSTALL_DATA) $$i $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/ || exit; \
				done; \
			); \
		done; \
		if test "$(ICU_FOR_APPLE_PLATFORMS)" = "YES"; then \
			if test ! -d $(DSTROOT)/$(HDR_PREFIX)/include/unicode/; then \
				$(INSTALL) -d -m 0755 $(DSTROOT)/$(HDR_PREFIX)/include/unicode/; \
			fi; \
			if test -d $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/; then \
				(cd $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode; \
					for i in *.h; do \
						if fgrep -q -x $$i $(SRCROOT)/apple/minimalapis.txt ; then \
							mv $$i $(DSTROOT)/$(HDR_PREFIX)/include/unicode ; \
						fi ; \
					done ); \
				if test ! "$(RC_XBS)" = "YES"; then \
					echo "# Not building for XBS, so running minimal test"; \
					$(CC) $(SRCROOT)/apple/minimalapisTest.c $(INSTALLHDRS_ARCH) $(ISYSROOT) -nostdinc \
						-I $(DSTROOT)/$(HDR_PREFIX)/include/ -I $(SDKPATH)/usr/include/ -E > /dev/null ; \
				fi; \
			fi; \
			$(UNIFDEF) $(UNIFDEF_FLAGS) -o $(OBJROOT_CURRENT)/ICU.modulemap         $(SRCROOT)/apple/modules/ICU.modulemap; \
			$(UNIFDEF) $(UNIFDEF_FLAGS) -o $(OBJROOT_CURRENT)/ICU.private.modulemap $(SRCROOT)/apple/modules/ICU.private.modulemap; \
			$(UNIFDEF) $(UNIFDEF_FLAGS) -o $(OBJROOT_CURRENT)/unicode.modulemap         $(SRCROOT)/apple/modules/unicode.modulemap; \
			$(UNIFDEF) $(UNIFDEF_FLAGS) -o $(OBJROOT_CURRENT)/unicode_private.modulemap $(SRCROOT)/apple/modules/unicode_private.modulemap; \
			$(INSTALL_DATA) $(OBJROOT_CURRENT)/ICU.modulemap         $(DSTROOT)/$(HDR_PREFIX)/include/unicode/module.modulemap; \
			$(INSTALL_DATA) $(OBJROOT_CURRENT)/ICU.private.modulemap $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode/module.modulemap; \
			$(INSTALL_DATA) $(OBJROOT_CURRENT)/unicode.modulemap         $(DSTROOT)/$(HDR_PREFIX)/include/unicode.modulemap; \
			$(INSTALL_DATA) $(OBJROOT_CURRENT)/unicode_private.modulemap $(DSTROOT)/$(PRIVATE_HDR_PREFIX)/include/unicode_private.modulemap; \
		fi; \
	);
installapi : installhdrsint 
	@echo
	@echo ++++++++++++++++++++++
	@echo + Running InstallAPI +
	@echo ++++++++++++++++++++++
	@echo
	 
	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
	  echo "installapi for target 'ICU' was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
	  exit 1; \
	fi
	 
	$(SRCROOT)/apple/generate_json_for_tapi.py $(SRCROOT) $(OBJROOT_CURRENT)/tapi_headers.json $(ICU_FOR_EMBEDDED_TRAINS)

	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_COMMON_OPTS)
	 
	$(INSTALL) -d -m 0755 $(DSTROOT)$(libdir)
	$(INSTALL) -c -m 0755 $(ICU_TBD_PATH) $(DSTROOT)$(libdir)/lib$(LIB_NAME).tbd

# We run configure and run make first. This generates the .o files. We then link them
# all up together into libicucore. Then we put it into its install location, create
# symbolic links, and then strip the main dylib. Then install the remaining libraries.
# We cleanup the sources folder.

ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
install : installhdrsint icu installapi-verify
else ifneq (,$(findstring --enable-fuzzer,$(OTHER_CONFIG_FLAGS)))
install : installhdrsint icu fuzzer
else
install : installhdrsint icu
endif 
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DSTROOT)/$(winprogdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(winprogdir)/; \
		fi; \
		if test ! -d $(DSTROOT)/$(winintlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(winintlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc.lib $(DSTROOT)/$(winintlibdir)libicuuc.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc.pdb $(DSTROOT)/$(winprogdir)libicuuc.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuuc.dll $(DSTROOT)/$(winprogdir)libicuuc.dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin.lib $(DSTROOT)/$(winintlibdir)libicuin.lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin.pdb $(DSTROOT)/$(winprogdir)libicuin.pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuin.dll $(DSTROOT)/$(winprogdir)libicuin.dll; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/icudt$(ICU_VERS).dll $(DSTROOT)/$(winprogdir)icudt$(ICU_VERS).dll; \
	else \
		if test ! -d $(DSTROOT)/$(libdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(libdir)/; \
		fi; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(DSTROOT)/$(libdir)$(INSTALLED_DYLIB); \
		if test ! -d $(SYMROOT_CURRENT)/; then \
			$(INSTALL) -d -m 0755 $(SYMROOT_CURRENT)/; \
		fi; \
		if test "$(LINUX)" = "YES"; then \
			cp $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			strip -x -S $(DSTROOT)/$(libdir)$(INSTALLED_DYLIB); \
		else \
			(cd $(DSTROOT)/$(libdir); \
			ln -fs  $(INSTALLED_DYLIB) $(DYLIB)); \
			cp $(OBJROOT_CURRENT)/$(INSTALLED_DYLIB) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(INSTALLED_DYLIB); \
			$(STRIPCMD) -x -u -r -S $(DSTROOT)/$(libdir)$(INSTALLED_DYLIB); \
		fi; \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT_CURRENT)/$$subdir; $(MAKE) -e DESTDIR=$(DSTROOT)/ $(ENV) install-library;) \
		done; \
		if test ! -d $(DSTROOT)/$(DATA_INSTALL_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(DATA_INSTALL_DIR)/; \
		fi; \
		if test -f $(OBJROOT_CURRENT)/$(L_DATA_FILE); then \
			$(INSTALL) -b -m 0444  $(OBJROOT_CURRENT)/$(L_DATA_FILE) $(DSTROOT)/$(DATA_INSTALL_DIR)$(L_DATA_FILE); \
		fi; \
		if test ! -d $(DSTROOT)/$(OPEN_SOURCE_VERSIONS_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(OPEN_SOURCE_VERSIONS_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DSTROOT)/$(OPEN_SOURCE_VERSIONS_DIR)ICU.plist; \
		if test ! -d $(DSTROOT)/$(OPEN_SOURCE_LICENSES_DIR)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(OPEN_SOURCE_LICENSES_DIR)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(SRCROOT)/icu/LICENSE $(DSTROOT)/$(OPEN_SOURCE_LICENSES_DIR)ICU.txt; \
		if test "$(LINUX)" != "YES"; then \
			if test -f $(OBJROOT_CURRENT)/$(TZDATA_FORMAT_FILE); then \
				$(INSTALL) -b -m 0644  $(OBJROOT_CURRENT)/$(TZDATA_FORMAT_FILE) $(DSTROOT)/$(DATA_INSTALL_DIR)$(TZDATA_FORMAT_FILE); \
			fi; \
			if test ! -d $(DSTROOT)/$(CLDRFILESDIR)/; then \
				$(INSTALL) -d -m 0755 $(DSTROOT)/$(CLDRFILESDIR)/; \
			fi; \
			$(INSTALL) -b -m 0644  $(SRCROOT)/apple/cldrFiles/supplementalData.xml $(DSTROOT)/$(CLDRFILESDIR)/supplementalData.xml; \
			$(INSTALL) -b -m 0644  $(SRCROOT)/apple/cldrFiles/plurals.xml $(DSTROOT)/$(CLDRFILESDIR)/plurals.xml; \
			if test ! -d $(DSTROOT)/$(EMOJI_DATA_DIR)/; then \
				$(INSTALL) -d -m 0755 $(DSTROOT)/$(EMOJI_DATA_DIR)/; \
			fi; \
			$(INSTALL) -b -m 0644 $(SRCROOT)/apple/emojiData/charClasses.txt $(DSTROOT)/$(EMOJI_DATA_DIR)/charClasses.txt; \
			$(INSTALL) -b -m 0644 $(SRCROOT)/apple/emojiData/lineClasses.txt $(DSTROOT)/$(EMOJI_DATA_DIR)/lineClasses.txt; \
			if test ! -d $(DSTROOT)/$(localtooldir)/; then \
				$(INSTALL) -d -m 0755 $(DSTROOT)/$(localtooldir)/; \
			fi; \
			if test -f $(OBJROOT_CURRENT)/$(INFOTOOL); then \
				echo '# install' $(INFOTOOL) 'to' $(DSTROOT)/$(localtooldir)$(INFOTOOL) ; \
				$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(INFOTOOL) $(DSTROOT)/$(localtooldir)$(INFOTOOL); \
				cp $(OBJROOT_CURRENT)/$(INFOTOOL) $(SYMROOT_CURRENT)/$(INFOTOOL); \
				$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(INFOTOOL)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(INFOTOOL); \
			fi; \
			if test ! -d $(DSTROOT)/$(locallibdir)/; then \
				$(INSTALL) -d -m 0755 $(DSTROOT)/$(locallibdir)/; \
			fi; \
			if test -f $(OBJROOT_CURRENT)/$(TOOLS_DYLIB); then \
				echo '# install' $(TOOLS_DYLIB) 'to' $(DSTROOT)/$(locallibdir)$(TOOLS_DYLIB) ; \
				$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/$(TOOLS_DYLIB) $(DSTROOT)/$(locallibdir)$(TOOLS_DYLIB); \
				cp $(OBJROOT_CURRENT)/$(TOOLS_DYLIB) $(SYMROOT_CURRENT)/$(TOOLS_DYLIB); \
				$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(TOOLS_DYLIB)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(TOOLS_DYLIB); \
			fi; \
			if test -f $(OBJROOT_CURRENT)/$(ICUZDUMPTOOL); then \
				echo '# install' $(ICUZDUMPTOOL) 'to' $(DSTROOT)/$(localtooldir)$(ICUZDUMPTOOL) ; \
				$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(ICUZDUMPTOOL) $(DSTROOT)/$(localtooldir)$(ICUZDUMPTOOL); \
				cp $(OBJROOT_CURRENT)/$(ICUZDUMPTOOL) $(SYMROOT_CURRENT)/$(ICUZDUMPTOOL); \
				$(DSYMTOOL) -o $(SYMROOT_CURRENT)/$(ICUZDUMPTOOL)$(DSYMSUFFIX) $(SYMROOT_CURRENT)/$(ICUZDUMPTOOL); \
			fi; \
		fi; \
	fi;

installapi-verify: installhdrs icu
	@echo
	@echo +++++++++++++++++++++++++++++++++
	@echo + Running InstallAPI and Verify +
	@echo ++++++++++++++++++++++++++++++++
	@echo

	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
	  echo "installapi for target 'ICU' was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
	  exit 1; \
	fi
	
	$(SRCROOT)/apple/generate_json_for_tapi.py $(SRCROOT) $(OBJROOT_CURRENT)/tapi_headers.json $(ICU_FOR_EMBEDDED_TRAINS)

	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_VERIFY_OPTS)

	$(INSTALL) -d -m 0755 $(DSTROOT)$(libdir)
	$(INSTALL) -c -m 0755 $(ICU_TBD_PATH) $(DSTROOT)$(libdir)/lib$(LIB_NAME).tbd

DEPEND_install_debug = debug
DEPEND_install_profile = profile

.SECONDEXPANSION:
install_debug install_profile : $$(DEPEND_$$@)
	if test "$(WINDOWS)" = "YES"; then \
		if test ! -d $(DSTROOT)/$(winprogdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(winprogdir)/; \
		fi; \
		if test ! -d $(DSTROOT)/$(winintlibdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(winintlibdir)/; \
		fi; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).lib $(DSTROOT)/$(winintlibdir)libicuuc_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).pdb $(DSTROOT)/$(winprogdir)libicuuc_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuuc_$(DEPEND_$@).dll $(DSTROOT)/$(winprogdir)libicuuc_$(DEPEND_$@).dll; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).lib $(DSTROOT)/$(winintlibdir)libicuin_$(DEPEND_$@).lib; \
		$(INSTALL) -b -m 0644 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).pdb $(DSTROOT)/$(winprogdir)libicuin_$(DEPEND_$@).pdb; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/lib/libicuin_$(DEPEND_$@).dll $(DSTROOT)/$(winprogdir)libicuin_$(DEPEND_$@).dll; \
	else \
		if test ! -d $(DSTROOT)/$(libdir)/; then \
			$(INSTALL) -d -m 0755 $(DSTROOT)/$(libdir)/; \
		fi; \
		$(INSTALL) -b -m 0664 $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(DSTROOT)/$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		if test "$(LINUX)" = "YES"; then \
			if test ! -d $(SYMROOT_CURRENT)/; then \
				$(INSTALL) -d -m 0755 $(SYMROOT_CURRENT)/; \
			fi; \
			cp $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			strip -x -S $(DSTROOT)/$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		else \
			(cd $(DSTROOT)/$(libdir); \
			ln -fs  $($(INSTALLED_DYLIB_$(DEPEND_$@))) $($(DYLIB_$(DEPEND_$@)))); \
			cp $(OBJROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))) $(SYMROOT_CURRENT)/$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
			$(STRIPCMD) -x -u -r -S $(DSTROOT)/$(libdir)$($(INSTALLED_DYLIB_$(DEPEND_$@))); \
		fi; \
		for subdir in $(EXTRA_LIBS); do \
			(cd $(OBJROOT_CURRENT)/$$subdir; $(MAKE) -e DESTDIR=$(DSTROOT)/ $(ENV) install-library;) \
		done; \
	fi;

clean :
	-rm -rf $(OBJROOT)

# For the install-icutztoolsforsdk target, SDKROOT will always be an OSX SDK root
install-icutztoolsforsdk : icutztoolsforsdk
ifeq "$(SDK_PLATFORM_NORMALIZED)" "macosx"
	if test ! -d $(DSTROOT)$(localtztooldir)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(localtztooldir)/; \
	fi;
	if test ! -d $(DSTROOT)$(locallibdir)/; then \
		$(INSTALL) -d -m 0755 $(DSTROOT)$(locallibdir)/; \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(TOOLS_DYLIB_FORTOOLS); then \
		echo '# install' $(TOOLS_DYLIB_FORTOOLS) 'to' $(DSTROOT)$(locallibdir)$(TOOLS_DYLIB_FORTOOLS) ; \
		$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/$(TOOLS_DYLIB_FORTOOLS) $(DSTROOT)$(locallibdir)$(TOOLS_DYLIB_FORTOOLS); \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(ZICTOOL); then \
		echo '# install' $(ZICTOOL) 'to' $(DSTROOT)$(localtztooldir)$(ZICTOOL) ; \
		$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(ZICTOOL) $(DSTROOT)$(localtztooldir)$(ZICTOOL); \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(RESTOOL); then \
		echo '# install' $(RESTOOL) 'to' $(DSTROOT)$(localtztooldir)$(RESTOOL) ; \
		$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(RESTOOL) $(DSTROOT)$(localtztooldir)$(RESTOOL); \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(PKGTOOL); then \
		echo '# install' $(PKGTOOL) 'to' $(DSTROOT)$(localtztooldir)$(PKGTOOL) ; \
		$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(PKGTOOL) $(DSTROOT)$(localtztooldir)$(PKGTOOL); \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(TZ2ICUTOOL); then \
		echo '# install' $(TZ2ICUTOOL) 'to' $(DSTROOT)$(localtztooldir)$(TZ2ICUTOOL) ; \
		$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(TZ2ICUTOOL) $(DSTROOT)$(localtztooldir)$(TZ2ICUTOOL); \
	fi;
	if test -f $(OBJROOT_CURRENT)/$(GENBRKTOOL); then \
		echo '# install' $(GENBRKTOOL) 'to' $(DSTROOT)$(localtztooldir)$(GENBRKTOOL) ; \
		$(INSTALL) -b -m 0755  $(OBJROOT_CURRENT)/$(GENBRKTOOL) $(DSTROOT)$(localtztooldir)$(GENBRKTOOL); \
	fi;
else
	echo "Dummy file to make verifiers happy (tzTools not supported on embedded platforms)" >$(OBJROOT_CURRENT)/README;
	$(INSTALL) -d -m 0755 $(DSTROOT)$(localtztooldir)/; \
	$(INSTALL) -b -m 0755 $(OBJROOT_CURRENT)/README $(DSTROOT)$(localtztooldir)README;
endif

icudata :
	cd $(SRCROOT)
	apple/scripts/build-icu-data.sh
