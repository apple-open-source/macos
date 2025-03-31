## New top-level makefile for ICU
# Copyright ©2024 Apple Inc.  All rights reserved.
#
# This file was created on 6/6/2024, supersedes the original ICU top-level makefile,
# and will (hopefully in the not-too-distant future) itself be superseded by a new
# Xcode project.
#
# This makefile attempts to streamline our build process and to make the makefile itself
# easier to read and maintain by eliminating all of the stuff for Windows and Linux
# and parts of the Apple-platform build process that are obsolete or redundant.
#################################

.PHONY : icu icuhost icudata check installsrc installhdrs installapi clean install install_debug \
	install-icutztoolsforsdk icu-separate-libs

#################################
# Global variable definitions
#################################
# directories
$(info # Start of makefile)
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
	SYMROOT:=$(OBJROOT)/syms
endif
$(info # SRCROOT=$(SRCROOT))
$(info # OBJROOT=$(OBJROOT))
$(info # DSTROOT=$(DSTROOT))

# Disallow $(SRCROOT) == $(OBJROOT)
ifeq ($(OBJROOT), $(SRCROOT))
$(error SRCROOT same as OBJROOT)
endif

HOSTSDKPATH := $(shell xcrun --sdk macosx.internal --show-sdk-path)
ifndef SDKROOT
	SDKROOT:=$(HOSTSDKPATH)
else ifeq "$(SDKROOT)" ""
	SDKROOT:=$(HOSTSDKPATH)
endif
SDKPATH:=$(shell xcodebuild -version -sdk $(SDKROOT) Path)
ifeq "$(SDKPATH)" ""
	SDKPATH:=/
endif

ifeq "$(SDKPATH)" "/"
	ISYSROOT:= -isysroot $(HOSTSDKPATH)
else
	ISYSROOT:= -isysroot $(SDKPATH)
endif
ifndef RC_ARCHS
	RC_ARCHS:=$(shell arch)
endif
$(info # SDKROOT=$(SDKROOT))
$(info # SDKPATH=$(SDKPATH))
$(info # HOSTSDKPATH=$(HOSTSDKPATH))
$(info # RC_ARCHS=$(RC_ARCHS))

ICU_SRC=$(SRCROOT)/icu/icu4c/source

#tools
INSTALL = /usr/bin/install
DSYMTOOL := /usr/bin/dsymutil
ifdef SDKPATH
	CC := $(shell xcrun --sdk $(SDKPATH) --find cc)
	CXX := $(shell xcrun --sdk $(SDKPATH) --find c++)
	STRIPCMD := $(shell xcrun --sdk $(SDKPATH) --find strip)
else
	CC := $(shell xcrun --sdk macosx --find cc)
	CXX := $(shell xcrun --sdk macosx --find c++)
	STRIPCMD := $(shell xcrun --sdk macosx --find strip)
endif
$(info # CC=$(CC))
$(info # CXX=$(CXX))

# if we're building under XBS, it doesn't pass the -j flag (which enables building different
# items together in parallel) down to `make`, so we use this to add it.  [For local builds,
# the user still has to supply the -j flag themselves-- we're using RC_PLATFORM_NAME as our
# way to tell if we're running this under XBS.]
ifdef RC_PLATFORM_NAME
	MAKE_J=-j
else
	MAKE_J=
endif

#versions and platforms
ICU_VERS = 76
ICU_SUBVERS = 1
CORE_VERS = A
TZDATA_FORMAT_STRING = "44l"

# Calculate the build number.  This replaces the more-complicated code in ll. 227-282 of the
# old makefile, but does more or less the same thing.  RC_ProjectSourceVersion is the submission
# number, which is in the format SSSSS.BB.TT where S is the main submission number, B is the
# branch number, and T is the train code.  The submission number further breaks down into MMmbb,
# where M is the major version number, m is the minor version number, and b is the build number.
# If RC_ProjectSourceVersion starts with ICU's actual major version number, then we assemble
# a build number that gets copied into some .plist files in the project, and that build number
# if a four digit number whose first two digits are the build number and whose last two digits
# are the branch number.  That is, if the submission name is "ICU-74215.12.34", the build number
# is "1512".  (We used to use the train code, but that's obsolete now.)  If RC_ProjectSourceVersion
# is empty or something else (pre-submission builds always use "99999.99.99"), we don't assemble
# a build number at all.
SPLIT_SUBMISSION=$(subst ., ,$(RC_ProjectSourceVersion)) # Change the periods to spaces so we can use the "word" function
SUBMISSION_NUMBER=$(word 1,$(SPLIT_SUBMISSION))
TMP_BRANCH_NUMBER=$(word 2,$(SPLIT_SUBMISSION))
ICU_TRAIN_CODE=$(word 3,$(SPLIT_SUBMISSION))
MAJOR_VERSION=$(shell echo $(SUBMISSION_NUMBER) | sed -E -e 's/([0-9]{2}).*/\1/') # the first two digits of the submission number
BUILD_VERSION=$(shell echo $(SUBMISSION_NUMBER) | sed -E -e 's/.*([0-9]{2})/\1/') # the last two digits of the submission number
BRANCH_NUMBER=$(shell echo 00$(TMP_BRANCH_NUMBER) | sed -E -e 's/.*([0-9]{2})/\1/') # the branch number, zero-padded to two digits (we add two zeros to the front and then take the rightmost two digits)
ifeq "$(MAJOR_VERSION)" "$(ICU_VERS)"
	# if the submission number agrees with the ICU version, ICU_BUILD is the two-digit build number
	# concatenated with the branch number (zero-padded to two digits).  DEFINE_BUILD_LEVEL adds a
	# define macro to the configuration environment passing that down into the ICU build.
	ICU_BUILD=$(BUILD_VERSION)$(BRANCH_NUMBER)
	DEFINE_BUILD_LEVEL =-DU_ICU_VERSION_BUILDLEVEL_NUM=$(ICU_BUILD)
else
	# Otherwise, we don't add the extra define macro to the configuration environment.
	ICU_BUILD=
	DEFINE_BUILD_LEVEL =
endif

# TODO(rtg): The old makefile had a bunch of complicated logic to figure out the right SDK verson to use;
# I want to go back to just hard-coding this if at all possible.  I think we might need to hard-code
# several different versions, but I'm hoping we don't need all that complicated logic to figure it
# out on the fly.
MAC_OS_X_VERSION_MIN_REQUIRED=150000
OSX_HOST_VERSION_MIN_STRING=15.0.0
# Turns out we DO need some of that logic, at least to make sure we're building icuhost (on non-macOS
# builds) with an SDK that's actually available on the build machine...
MACOS_BLDHOST_SDK_VERSION=$(shell xcodebuild -version -sdk macosx.internal SDKVersion)
OSX_BLDHOST_VERSION_MIN_STRING=$(MACOS_BLDHOST_SDK_VERSION).0
BLDHOST_VERSION_MIN_REQUIRED=$(shell echo $(MACOS_BLDHOST_SDK_VERSION) | cut -f1 -d.)0$(shell echo $(MACOS_BLDHOST_SDK_VERSION) | cut -f2 -d.)00
$(info # OSX_BLDHOST_VERSION_MIN_STRING=$(OSX_BLDHOST_VERSION_MIN_STRING))
$(info # BLDHOST_VERSION_MIN_REQUIRED=$(BLDHOST_VERSION_MIN_REQUIRED))

ifndef RC_PLATFORM_NAME
	RC_PLATFORM_NAME=MacOSX
endif
$(info # RC_PLATFORM_NAME=$(RC_PLATFORM_NAME))

ifeq "$(RC_PLATFORM_NAME)" "iPhoneOS"
	TARGET_TRIPLE_SYS=ios
	NORMALIZED_PLATFORM=iphoneos
else ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
	TARGET_TRIPLE_SYS=macos
	NORMALIZED_PLATFORM=macosx
else ifeq "$(RC_PLATFORM_NAME)" "AppleTVOS"
	TARGET_TRIPLE_SYS=tvos
	NORMALIZED_PLATFORM=appletvos
else ifeq "$(RC_PLATFORM_NAME)" "WatchOS"
	TARGET_TRIPLE_SYS=watchos
	NORMALIZED_PLATFORM=watchos
else ifeq "$(RC_PLATFORM_NAME)" "XROS"
	TARGET_TRIPLE_SYS=xros
	NORMALIZED_PLATFORM=xros
else ifeq "$(RC_PLATFORM_NAME)" "BridgeOS"
	TARGET_TRIPLE_SYS=bridgeos
	NORMALIZED_PLATFORM=bridgeos
endif

ifndef SDKVERSION
	export SDKVERSION := $(shell xcodebuild -version -sdk $(NORMALIZED_PLATFORM) SDKVersion | head -1)
endif
$(info # SDKVERSION=$(SDKVERSION))

# When we're building for macOS, we need the iOS SDK version to pass in the -target-variant parameter
# to enable building for MacCatalyst.  We fetch that out of the SDK_VARIANT_VERSION_MAP, which is
# passed to us by the build system (if we're not just running "make" locally).
ifdef SDK_VARIANT_VERSION_MAP
    SDK_VERSION_ESCAPED := $(shell echo '$(SDKVERSION)' | sed 's/\./\\./g')
    IOS_SDK_VERSION := $(shell echo '$(SDK_VARIANT_VERSION_MAP)' | sed -E 's/.*(^| )$(SDK_VERSION_ESCAPED):([\.0-9]+)( |$$).*/\2/' )
else
	IOS_SDK_VERSION := $(shell xcodebuild -version -sdk iphoneos SDKVersion | head -1)
endif
$(info # IOS_SDK_VERSION=$(IOS_SDK_VERSION))

CFLAGS_SANITIZER :=
CXXFLAGS_SANITIZER :=
LDFLAGS_SANITIZER := -Wl,-not_for_dyld_shared_cache,-no_warn_inits
ifeq ($(RC_ENABLE_ADDRESS_SANITIZATION),1)
  CFLAGS_SANITIZER += -fsanitize=address
  CXXFLAGS_SANITIZER += -fsanitize=address
  LDFLAGS_SANITIZER += -fsanitize=address
  LDFLAGS += $(LDFLAGS_SANITIZER)
endif
ifeq ($(RC_ENABLE_UNDEFINED_BEHAVIOR_SANITIZATION),1)
  CFLAGS_SANITIZER += -fsanitize=undefined
  CXXFLAGS_SANITIZER += -fsanitize=undefined
  LDFLAGS_SANITIZER += -fsanitize=undefined
  LDFLAGS += $(LDFLAGS_SANITIZER)
endif
ifeq ($(RC_ENABLE_THREAD_SANITIZATION),1)
  CFLAGS_SANITIZER += -fsanitize=thread
  CXXFLAGS_SANITIZER += -fsanitize=thread
  LDFLAGS_SANITIZER += -fsanitize=thread
  LDFLAGS += $(LDFLAGS_SANITIZER)
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
ifndef TZDATA
	# if we didn't find the TZDATA directory in the SDK and we're building for the simulator,
	# try the non-simulator SDK directory-- sometimes the time zone stuff is omitted from
	# the simulator SDKs
	ifeq "$(RC_ProjectName)" "ICU_Sim"
		TZDATA_SDKPATH:=$(shell xcodebuild -version -sdk $(NORMALIZED_PLATFORM).internal Path)
		ifeq "$(shell test -L $(TZDATA_SDKPATH)/usr/local/share/tz/latest_tzdata.tar.gz && echo YES )" "YES"
			export TZDATA:=$(TZDATA_SDKPATH)/usr/local/share/tz/$(shell readlink $(TZDATA_SDKPATH)/usr/local/share/tz/latest_tzdata.tar.gz)
		endif
		ifeq "$(shell test -d $(TZDATA_SDKPATH)/usr/local/share/tz/icudata && echo YES )" "YES"
			export TZAUXFILESDIR:=$(TZDATA_SDKPATH)/usr/local/share/tz/icudata
		endif
	endif
endif
$(info # RC_EMBEDDEDPROJECT_DIR=$(RC_EMBEDDEDPROJECT_DIR))
$(info # TZDATA=$(TZDATA))
$(info # TZAUXFILESDIR=$(TZAUXFILESDIR))
$(info # TZDATA_SDKPATH=$(TZDATA_SDKPATH))

.DEFAULT_GOAL=icu

#################################
# installsrc target
# (NOTE: The official build process doesn't use the installsrc target anymore, but the buildit
# tool still does.  Make sure that any changes to INSTALLSRC_CONTENTS below are also made to
# the .submitproject file.)
#################################
INSTALLSRC_CONTENTS=./makefile ./ICU.plist ./icu/LICENSE ./icu/icu4c/source ./apple ./ICU_embedded.order

installsrc :
	mkdir -p $(SRCROOT)
	rm -rf $(SRCROOT)/icu/icu4c/source
	tar cf - $(INSTALLSRC_CONTENTS) | (cd $(SRCROOT) ; tar xfp -);

#################################
# installhdrs target
#################################
PUBLIC_MODULEMAP_PATH=$(DSTROOT)/usr/include
PRIVATE_MODULEMAP_PATH=$(DSTROOT)/usr/local/include
PUBLIC_HDR_PATH=$(PUBLIC_MODULEMAP_PATH)/unicode
PRIVATE_HDR_PATH=$(PRIVATE_MODULEMAP_PATH)/unicode

PUBLIC_HEADERS=localpointer.h parseerr.h platform.h ptypes.h putil.h stringoptions.h uchar.h \
	uconfig.h ucpmap.h uidna.h uiter.h umachine.h uregex.h urename.h ustring.h utext.h utf.h \
	utf_old.h utf16.h utf8.h utypes.h uvernum.h uversion.h

ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
UNIFDEF_FLAGS=-DBUILD_FOR_MACOS -UBUILD_FOR_EMBEDDED
else
UNIFDEF_FLAGS=-DBUILD_FOR_EMBEDDED -UBUILD_FOR_MACOS
endif

ifeq "$(RC_ProjectName)" "tzTools"
installhdrs :
	# don't do anything if we're building tzTools
else
installhdrs :
	mkdir -p $(PRIVATE_HDR_PATH)
	cp $(ICU_SRC)/common/unicode/*.h $(PRIVATE_HDR_PATH)
	cp $(ICU_SRC)/i18n/unicode/*.h $(PRIVATE_HDR_PATH)
	if [ "$(RC_PLATFORM_NAME)" == "MacOSX" ]; then cp $(ICU_SRC)/io/unicode/*.h $(PRIVATE_HDR_PATH); fi;
	mkdir -p $(PUBLIC_HDR_PATH)
	mv $(PUBLIC_HEADERS:%=$(PRIVATE_HDR_PATH)/%) $(PUBLIC_HDR_PATH)
	unifdef $(UNIFDEF_FLAGS) -x 2 -o $(PUBLIC_MODULEMAP_PATH)/unicode.modulemap $(SRCROOT)/apple/modules/unicode.modulemap
	unifdef $(UNIFDEF_FLAGS) -x 2 -o $(PRIVATE_MODULEMAP_PATH)/unicode_private.modulemap $(SRCROOT)/apple/modules/unicode_private.modulemap
endif

#################################
# installapi target
#################################

DYLIB_NAME=libicucore.$(CORE_VERS).dylib

ifeq ($(RC_ProjectName), $(filter $(RC_ProjectName),ICU ICU_Sim))
  SUPPORTS_TEXT_BASED_API ?= YES
  $(info # SUPPORTS_TEXT_BASED_API=$(SUPPORTS_TEXT_BASED_API))
endif # RC_ProjectName 

ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
  # Create complete target triples for tapi and pass them all at once. 
  ifeq "$(RC_ProjectName)" "ICU_Sim"
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${SDKVERSION}-simulator, ${RC_ARCHS})
  else ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${OSX_HOST_VERSION_MIN_STRING}, ${RC_ARCHS})
    TARGET_TRIPLES := $(TARGET_TRIPLES) $(patsubst %, -target-variant ${ARCH}%-apple-ios$(IOS_SDK_VERSION)-macabi, ${RC_ARCHS})
  else
    TARGET_TRIPLES := $(patsubst %, -target ${ARCH}%-apple-${TARGET_TRIPLE_SYS}${SDKVERSION}, ${RC_ARCHS})
  endif
  
  
  ICU_TBD_PATH = $(OBJROOT)/usr/lib/lib$(LIB_NAME).tbd
  TAPI_COMMON_OPTS := -dynamiclib \
  	-xc++ -std=c++17 \
  	$(TARGET_TRIPLES) \
  	-fvisibility=hidden \
  	-isysroot $(SDKROOT) \
  	-iquote $(SRCROOT)/icu/icu4c/source \
  	-iquote $(SRCROOT)/icu/icu4c/source/common \
  	-iquote $(SRCROOT)/icu/icu4c/source/i18n \
  	-install_name /usr/lib/libicucore.$(CORE_VERS).dylib \
  	-current_version $(ICU_VERS).$(ICU_SUBVERS) \
    -compatibility_version 1 \
  	-DICU_DATA_DIR=\"/usr/share/icu\" \
  	-DU_SHOW_INTERNAL_API=1 \
  	-DU_DISABLE_RENAMING=1 \
  	-DU_SHOW_CPLUSPLUS_API=1 \
  	-DU_DEFAULT_SHOW_DRAFT=0 \
  	-DU_SHOW_DRAFT_API \
  	-DU_HAVE_STRTOD_L=1 \
  	-DU_HAVE_XLOCALE_H=1 \
  	-DU_HAVE_STRING_VIEW=1 \
  	-DU_COMBINED_IMPLEMENTATION=1 \
  	-DU_ALL_IMPLEMENTATION=1 \
  	-DU_LOCAL_SERVICE_HOOK=1 \
  	-DU_TIMEZONE=timezone \
  	-DUCONFIG_NO_MF2=1 \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dtitv_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dt_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/dtptngen_impl.h \
  	-exclude-project-header $(SRCROOT)/icu/icu4c/source/i18n/selfmtimpl.h \
    -o $(ICU_TBD_PATH) \
  	-filelist $(OBJROOT)/tapi_headers.json
  
  TAPI_VERIFY_OPTS := $(TAPI_COMMON_OPTS) \
              --verify-mode=Pedantic \
              --verify-against=$(OBJROOT)/$(DYLIB_NAME)
endif # SUPPORTS_TEXT_BASED_API

# NOTE: Again, the reason why we care about embedded builds is because we exclude the io library
# on iOS but not on Mac.  If we either shipped it everywhere or excluded it everywhere, we could
# get rid of this. --rtg 6/7/24
ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
EMBEDDED_BUILD="NO"
else
EMBEDDED_BUILD="YES"
endif

installapi : installhdrs
	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
	  echo "installapi for target 'ICU' was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
	  exit 1; \
	fi
	$(SRCROOT)/apple/generate_json_for_tapi.py $(SRCROOT) $(OBJROOT)/tapi_headers.json $(EMBEDDED_BUILD)
	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_COMMON_OPTS)
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/lib
	$(INSTALL) -c -m 0755 $(ICU_TBD_PATH) $(DSTROOT)/usr/lib/libicucore.tbd
	
installapi-verify : installapi icu
	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_VERIFY_OPTS)
	  

#################################
# install target
#################################
DATA_FILE_NAME=icudt$(ICU_VERS)l.dat

install : ENV=$(ENV_release)
install : installhdrs installapi icu icuhost installapi-verify
	@echo "Install libicucore.dylib to /usr/lib"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/lib
	$(INSTALL) -d -m 0755 $(SYMROOT)
	$(INSTALL) -b -m 0755 $(OBJROOT)/$(DYLIB_NAME) $(DSTROOT)/usr/lib/$(DYLIB_NAME)
	cd $(DSTROOT)/usr/lib ; ln -fs  $(DYLIB_NAME) libicucore.dylib
	cp $(OBJROOT)/$(DYLIB_NAME) $(SYMROOT)/$(DYLIB_NAME)
	$(DSYMTOOL) -o $(SYMROOT)/$(DYLIB_NAME).dSYM $(SYMROOT)/$(DYLIB_NAME)
	$(STRIPCMD) -x -u -r -S $(DSTROOT)/usr/lib/$(DYLIB_NAME)
	@echo "Install ICU.plist to /usr/local/OpenSourceVersions"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/OpenSourceVersions/
	$(INSTALL) -b -m 0644 $(SRCROOT)/ICU.plist $(DSTROOT)/usr/local/OpenSourceVersions/ICU.plist
	@echo "Install LICENSE (ICU.txt) to /usr/local/OpenSourceLicenses"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/OpenSourceLicenses/
	$(INSTALL) -b -m 0644 $(SRCROOT)/icu/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/ICU.txt
	@echo "Install supplementalData.xml and plurals.xml to /usr/local/share/cldr"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/share/cldr/
	$(INSTALL) -b -m 0644 $(SRCROOT)/apple/cldrFiles/supplementalData.xml $(DSTROOT)/usr/local/share/cldr/supplementalData.xml
	$(INSTALL) -b -m 0644 $(SRCROOT)/apple/cldrFiles/plurals.xml $(DSTROOT)/usr/local/share/cldr/plurals.xml
	@echo "Install charClasses.txt and lineClasses.txt to /usr/local/share/emojiData"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/share/emojiData/
	$(INSTALL) -b -m 0644 $(SRCROOT)/apple/emojiData/charClasses.txt $(DSTROOT)/usr/local/share/emojiData/charClasses.txt
	$(INSTALL) -b -m 0644 $(SRCROOT)/apple/emojiData/lineClasses.txt $(DSTROOT)/usr/local/share/emojiData/lineClasses.txt
	@echo "Install icuinfo tool to /usr/local/bin"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/bin/
	$(INSTALL) -b -m 0755 $(OBJROOT)/bin/icuinfo $(DSTROOT)/usr/local/bin/icuinfo
	cp $(OBJROOT)/bin/icuinfo $(SYMROOT)/icuinfo
	$(DSYMTOOL) -o $(SYMROOT)/icuinfo.dSYM $(SYMROOT)/icuinfo
	@echo "Install libicutu.dylib to /usr/local/lib"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/local/lib/
	$(INSTALL) -b -m 0755 $(OBJROOT)/libicutu.dylib $(DSTROOT)/usr/local/lib/libicutu.dylib
	cp $(OBJROOT)/libicutu.dylib $(SYMROOT)/libicutu.dylib
	$(DSYMTOOL) -o $(SYMROOT)/libicutu.dylib.dSYM $(SYMROOT)/libicutu.dylib
	@echo "Install icuzdump tool to /usr/local/bin"
	$(INSTALL) -b -m 0755  $(OBJROOT)/bin/icuzdump $(DSTROOT)/usr/local/bin/icuzdump
	cp $(OBJROOT)/bin/icuzdump $(SYMROOT)/icuzdump
	$(DSYMTOOL) -o $(SYMROOT)/icuzdump.dSYM $(SYMROOT)/icuzdump
	@echo "Install ICU data file to /usr/share/icu"
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/share/icu
	$(INSTALL) -b -m 0444 $(OBJROOT)/icuhost/data/out/$(DATA_FILE_NAME) $(DSTROOT)/usr/share/icu
	printf $(TZDATA_FORMAT_STRING) > $(DSTROOT)/usr/share/icu/icutzformat.txt
	@echo "Install ICU.plist to /System/Library/FeatureFlags/Domain"
	$(INSTALL) -d -m 0755 $(DSTROOT)/System/Library/FeatureFlags/Domain
	$(INSTALL) -b -m 0444 $(SRCROOT)/apple/featureFlags/ICU.plist $(DSTROOT)/System/Library/FeatureFlags/Domain

#################################
# install_debug target
#################################
DEBUG_DYLIB_NAME=libicucore.$(CORE_VERS)_debug.dylib

install_debug : ENV=$(ENV_debug)
install_debug : $(OBJROOT)/libicucore.dylib
	$(INSTALL) -d -m 0755 $(DSTROOT)/usr/lib
	$(INSTALL) -b -m 0755 $(OBJROOT)/$(DYLIB_NAME) $(DSTROOT)/usr/lib/$(DEBUG_DYLIB_NAME)
	cd $(DSTROOT)/usr/lib ; ln -fs  $(DEBUG_DYLIB_NAME) libicucore_debug.dylib
	cp $(OBJROOT)/$(DYLIB_NAME) $(SYMROOT)/$(DEBUG_DYLIB_NAME)
	$(STRIPCMD) -x -u -r -S $(DSTROOT)/usr/lib/$(DEBUG_DYLIB_NAME)

#################################
# install-icutztoolsforsdk target
#################################
ifeq "$(RC_PLATFORM_NAME)" "BridgeOS"
install-icutztoolsforsdk : 
	# don't build tzTools if we're building for bridgeOS
	# but write a dummy file to the output directory to keep the verifiers happy
	$(INSTALL) -d -m 0755 $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin
	echo "tzTools isn't actually supported on bridgeOS" >$(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/tzTools.readme.txt
else
install-icutztoolsforsdk : ENV=$(ENV_release)
install-icutztoolsforsdk : tztools
	$(INSTALL) -d -m 0755 $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/lib
	$(INSTALL) -b -m 0755 $(OBJROOT)/libicutux.dylib $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/lib/libicutux.dylib
	$(INSTALL) -d -m 0755 $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin
	$(INSTALL) -b -m 0755 $(OBJROOT)/icuzic $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/icuzic
	$(INSTALL) -b -m 0755 $(OBJROOT)/icugenrb $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/icugenrb
	$(INSTALL) -b -m 0755 $(OBJROOT)/icupkg $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/icupkg
	$(INSTALL) -b -m 0755 $(OBJROOT)/tz2icu $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/tz2icu
	$(INSTALL) -b -m 0755 $(OBJROOT)/icugenbrk $(DSTROOT)/$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin/icugenbrk
endif
	
#################################
# clean target
#################################
clean :
	-rm -rf $(OBJROOT)

#################################
# all target
#################################
all :
	@echo "The ICU makefile doesn't support the 'all' target.  Use 'make icu' or 'make check'."

#################################
# icu target
# This is the main target for building ICU
#################################
BUILD_SRC=$(SRCROOT)/icu/icu4c/source
BUILD_OUT=$(OBJROOT)
HOST_BUILD_OUT=$(BUILD_OUT)/icuhost
PRIVATE_HDR_PREFIX=/usr/local
DATA_LOOKUP_DIR=/usr/share/icu
TZDATA_LOOKUP_DIR = /var/db/timezone/icutz
TZDATA_PACKAGE = icutz44l
APPLE_HARDENING_OPTS := -D_FORTIFY_SOURCE=2 -fstack-protector-strong
APPLE_STACK_INIT_OPTS := -ftrivial-auto-var-init=zero
BUILD_MACHINE_ARCH=$(shell uname -m)
$(info # BUILD_MACHINE_ARCH=$(BUILD_MACHINE_ARCH))
RC_ARCHS_FIRST=$(shell echo $(RC_ARCHS) | cut -d' ' -f1)
# If we're on an x86_64 build machine and x86_64 is in $(RC_ARCHS),
# use it, whether it's listed first or not, when running runConfigureICU.
ifeq ($(BUILD_MACHINE_ARCH), x86_64)
	ifneq ($(findstring x86_64, $(RC_ARCHS)),)
		RC_ARCHS_FIRST=x86_64
	endif
endif
ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
	ENV_CONFIGURE_ARCHS=-arch $(RC_ARCHS_FIRST)
	ICU_TARGET_VERSION := $(TARGET_TRIPLES)
	CROSS_BUILD_OPTIONS=
	MACOSX_MIN_VERSION_OPTIONS=-DMAC_OS_X_VERSION_MIN_REQUIRED=$(MAC_OS_X_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_HOST_VERSION_MIN_STRING)
else ifeq "$(RC_ProjectName)" "ICU_Sim"
	ENV_CONFIGURE_ARCHS=-arch $(RC_ARCHS_FIRST)
	ICU_TARGET_VERSION := -target unknown-apple-$(TARGET_TRIPLE_SYS)$(SDKVERSION)-simulator
	CROSS_BUILD_OPTIONS="--host=$(RC_ARCHS_FIRST)-apple-darwin21.0.0 --with-cross-build=$(HOST_BUILD_OUT)"
	MACOSX_MIN_VERSION_OPTIONS=
else # ICU, but on iOS
	ENV_CONFIGURE_ARCHS=-arch $(RC_ARCHS_FIRST)
	ICU_TARGET_VERSION := -target unknown-apple-$(TARGET_TRIPLE_SYS)$(SDKVERSION)
	CROSS_BUILD_OPTIONS="--host=$(RC_ARCHS_FIRST)-apple-darwin21.0.0 --with-cross-build=$(HOST_BUILD_OUT)"
	MACOSX_MIN_VERSION_OPTIONS=
endif
$(info # RC_ARCHS_FIRST=$(RC_ARCHS_FIRST))

CONFIG_FLAGS = --disable-renaming --disable-extras --disable-layout --disable-samples \
	--with-data-packaging=archive --prefix=$(PRIVATE_HDR_PREFIX)  $(OTHER_CONFIG_FLAGS)

# TODO(rtg): ENV_CONFIGURE and ENV are _almost_ the same, but there are differences in the compiler
# flags.  Once we get everything working right, consider simplifying this so that the common stuff
# is only in one place (unless we've got a good reason to keep them entirely separate like this)
ENV_CONFIGURE= \
	CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED $(ISYSROOT) $(ENV_CONFIGURE_ARCHS)" \
	CC="$(CC)" \
	CXX="$(CXX)" \
	CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(APPLE_HARDENING_OPTS) $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CFLAGS_SANITIZER)" \
	CXXFLAGS="--std=c++17 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(ENV_CONFIGURE_ARCHS) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CXXFLAGS_SANITIZER)" \
	TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
	RC_ARCHS="$(RC_ARCHS)" \
	TZDATA="$(TZDATA)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

ENV_release= \
	CC="$(CC)" \
	CXX="$(CXX)" \
	CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(APPLE_HARDENING_OPTS) $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CFLAGS_SANITIZER)" \
	CXXFLAGS="--std=c++17 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CXXFLAGS_SANITIZER)" \
	TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
	RC_ARCHS="$(RC_ARCHS)" \
	TZDATA="$(TZDATA)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

ENV_debug= \
	CC="$(CC)" \
	CXX="$(CXX)" \
	CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(APPLE_HARDENING_OPTS) $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CFLAGS_SANITIZER)" \
	CXXFLAGS="--std=c++17 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" $(MACOSX_MIN_VERSION_OPTIONS) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -O0 -gfull -Wglobal-constructors -Wformat-nonliteral -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(APPLE_STACK_INIT_OPTS) $(ISYSROOT) $(CXXFLAGS_SANITIZER)" \
	TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
	RC_ARCHS="$(RC_ARCHS)" \
	TZDATA="$(TZDATA)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

# On the Mac, libicucore includes everything in libicuio; on iOS et al. it doesn't.
# That means that on iOS et al., we have to include the libicuio stuff directly in icuzdump.
ifeq "$(RC_PLATFORM_NAME)" "MacOSX"
	IO_OBJS_FOR_ICUCORE=$(BUILD_OUT)/io/*.o
	IO_OBJS_FOR_ICUZDUMP=
else
	IO_OBJS_FOR_ICUCORE=
	IO_OBJS_FOR_ICUZDUMP=$(BUILD_OUT)/io/*.o
endif

$(BUILD_OUT)/makefile :
	mkdir -p $(BUILD_OUT)
	cd $(BUILD_OUT); $(ENV_CONFIGURE) $(BUILD_SRC)/runConfigureICU MacOSX --srcdir=$(BUILD_SRC) $(CROSS_BUILD_OPTIONS) $(CONFIG_FLAGS) || ( echo "Error in runConfigureICU.  Contents of config.log:"; cat config.log; exit 1);
	# Manually copy Makefile.local over to the build-output directory (the old makefile
	# did this by copying ALL THE SOURCE over to the build output directory)
	cp $(BUILD_SRC)/common/Makefile.local $(BUILD_OUT)/common
	cp $(BUILD_SRC)/i18n/Makefile.local $(BUILD_OUT)/i18n
	
icu-separate-libs : $(BUILD_OUT)/makefile
	mkdir -p $(BUILD_OUT)/lib;
	cd $(BUILD_OUT)/stubdata; $(MAKE) $(MAKE_J) $(ENV) $(LIBOVERRIDES);
	cd $(BUILD_OUT)/common; $(MAKE) $(MAKE_J) $(ENV) $(LIBOVERRIDES);
	cd $(BUILD_OUT)/i18n; $(MAKE) $(MAKE_J) $(ENV) $(LIBOVERRIDES);
	cd $(BUILD_OUT)/io; $(MAKE) $(MAKE_J) $(ENV) $(LIBOVERRIDES);
	if [ $(BUILD_OUT)/lib/libicuuc.dylib -nt $(BUILD_OUT)/lib_mod_date ]; then touch $(BUILD_OUT)/lib_mod_date; fi;
	if [ $(BUILD_OUT)/lib/libicui18n.dylib -nt $(BUILD_OUT)/lib_mod_date ]; then touch $(BUILD_OUT)/lib_mod_date; fi;
	if [ $(BUILD_OUT)/lib/libicuio.dylib -nt $(BUILD_OUT)/lib_mod_date ]; then touch $(BUILD_OUT)/lib_mod_date; fi;

# NOTE: We can't rely on "make"'s dependency management here to tell us when to rebuild libicucore.dylib--
# we need to always run OSICU's makefiles, and then afterwards see if those makefiles actually updated
# their respective libraries.  This, of course, wouldn't be necessary if this makefile just built
# libicucore.dylib directly from source, rather than using OSICU's makefile and then trying to repackage
# everything.  (I hope to solve this with an Xcode project rather than by making further tweaks to
# this makefile.)
$(BUILD_OUT)/libicucore.dylib : icu-separate-libs
	if [ $(BUILD_OUT)/lib_mod_date -nt $(BUILD_OUT)/libicucore.$(CORE_VERS).dylib ]; then \
		echo "Building composite libicucore library" ; \
		$($(ENV)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
			$(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) \
			$(CXXFLAGS) $(LDFLAGS) -dead_strip \
			-install_name /usr/lib/libicucore.$(CORE_VERS).dylib -o $(BUILD_OUT)/libicucore.$(CORE_VERS).dylib \
			$(BUILD_OUT)/stubdata/*.o $(BUILD_OUT)/common/*.o $(BUILD_OUT)/i18n/*.o $(IO_OBJS_FOR_ICUCORE) ; \
		ln -fs  $(BUILD_OUT)/libicucore.$(CORE_VERS).dylib $(BUILD_OUT)/libicucore.dylib ; \
	fi;
	
# NOTE: This target does the same basic thing as the libicucore target above, but is rebuilding libicutu
# to link to libicucore instead of the individual ICU libraries.  Again, we could build straight from source
# instead of using OSICU's makefiles, but that seems better done as part of moving to an Xcode project.
$(BUILD_OUT)/libicutu.dylib : $(BUILD_OUT)/libicucore.dylib
	cd $(BUILD_OUT)/tools/toolutil; $(MAKE) $(ENV) $(LIBOVERRIDES);
	$($(ENV)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
		$(RC_ARCHS:%=-arch %) $(ICU_TARGET_VERSION) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) \
		$(CXXFLAGS) $(LDFLAGS) \
		-install_name /usr/local/lib/libicutu.dylib -o $(BUILD_OUT)/libicutu.dylib $(BUILD_OUT)/tools/toolutil/*.o \
		-L$(BUILD_OUT) -licucore ;

# NOTE: We're also using the OSICU makefile to build icuinfo, which we're also rebuilding here so
# that it depends on libicucore.  I tried changing this target to build icuinfo from source directly,
# but ran into too many errors and gave up.
$(BUILD_OUT)/bin/icuinfo : $(BUILD_OUT)/libicucore.dylib $(BUILD_OUT)/libicutu.dylib
	mkdir -p $(BUILD_OUT)/bin
	cd $(BUILD_OUT)/tools/icuinfo; $(MAKE) $(ENV) $(LIBOVERRIDES);
	$($(ENV)) $(CXX) --std=c++17 $(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) \
		-dead_strip -o $(BUILD_OUT)/bin/icuinfo $(BUILD_OUT)/tools/icuinfo/icuinfo.o \
		$(CXXFLAGS) $(LDFLAGS) \
		$(BUILD_OUT)/tools/toolutil/udbgutil.o $(BUILD_OUT)/tools/toolutil/uoptions.o \
		-L$(BUILD_OUT) -licucore ;

# NOTE: This target DOES build icuzdump directly from source, mostly because I couldn't figure
# out how to do it using OSICU's makefiles (I have no idea how the old makefile did this).
# Manually defining U_SHOW_CPLUSPLUS_API and U_OVERRIDE is definitely not cool, but was
# the path of least resistance.  Again, it makes sense to revisit this as part of moving
# to an Xcode project.
$(BUILD_OUT)/bin/icuzdump : $(BUILD_OUT)/libicucore.dylib $(BUILD_OUT)/libicutu.dylib
	mkdir -p $(BUILD_OUT)/bin
	$($(ENV)) $(LINK.cc) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) --std=c++17 \
		-dead_strip -o $(BUILD_OUT)/bin/icuzdump $(BUILD_SRC)/tools/tzcode/icuzdump.cpp $(IO_OBJS_FOR_ICUZDUMP) \
		$(CXXFLAGS) $(LDFLAGS) \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/io -I$(BUILD_SRC)/tools/toolutil \
		-DICU_DATA_DIR -DU_SHOW_CPLUSPLUS_API -DU_OVERRIDE=override \
		-L$(BUILD_OUT) -licutu -licucore ;

icu : icuhost $(BUILD_OUT)/libicucore.dylib $(BUILD_OUT)/libicutu.dylib $(BUILD_OUT)/bin/icuinfo $(BUILD_OUT)/bin/icuzdump
	
#################################
# icuhost target
# This target builds all of the regular ICU makefile's stuff (via the "data" target) on the build-host
# OS.  We use this to build the data file-- the data-build tools have to build and run on the build host,
# even when we're doing a cross-platform build, and on Mac we need a non-zippered ICU build in order to
# build the data-build tools without having to code-sign anything.
#################################

HOSTCC := $(shell xcrun --sdk macosx --find cc)
HOSTCXX := $(shell xcrun --sdk macosx --find c++)
HOSTSDKPATH := $(shell xcrun --sdk macosx.internal --show-sdk-path)
HOSTISYSROOT = -isysroot $(HOSTSDKPATH)
$(info # HOSTCC=$(HOSTCC))
$(info # HOSTCXX=$(HOSTCXX))

# TODO(rtg): This works, but the data file it produces isn't identical to the
# data file I was getting before.  The differences appear to be time-zone-related, but I'm not sure.
# Figure out why they're different and how to get them to be the same (or satisfy yourself that they
# don't have to be the same).

ENV_CONFIGURE_BUILDHOST= \
	CPPFLAGS="$(DEFINE_BUILD_LEVEL) -DSTD_INSPIRED $(HOSTISYSROOT)" \
	CC="$(HOSTCC)" \
	CXX="$(HOSTCXX)" \
	CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(BLDHOST_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_BLDHOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
	CXXFLAGS="--std=c++17 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(BLDHOST_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_BLDHOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
	TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
	TZDATA="$(TZDATA)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

ENV_BUILDHOST= \
	CC="$(HOSTCC)" \
	CXX="$(HOSTCXX)" \
	CFLAGS="-DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(BLDHOST_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_BLDHOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden" \
	CXXFLAGS="--std=c++17 -DU_SHOW_CPLUSPLUS_API=1 -DU_SHOW_INTERNAL_API=1 -DU_TIMEZONE=timezone -DICU_DATA_DIR=\"\\\"$(DATA_LOOKUP_DIR)\\\"\" -DMAC_OS_X_VERSION_MIN_REQUIRED=$(BLDHOST_VERSION_MIN_REQUIRED) -mmacosx-version-min=$(OSX_BLDHOST_VERSION_MIN_STRING) -DU_TIMEZONE_FILES_DIR=\"\\\"$(TZDATA_LOOKUP_DIR)\\\"\" -DU_TIMEZONE_PACKAGE=\"\\\"$(TZDATA_PACKAGE)\\\"\" -DUCONFIG_NO_MF2=1 $(HOSTISYSROOT) -g -Os -Wglobal-constructors -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden" \
	TOOLSEXTRAFLAGS="$(TOOLSEXTRAFLAGS)" \
	TZDATA="$(TZDATA)" \
	DYLD_LIBRARY_PATH="$(DSTROOT)/usr/local/lib"

$(HOST_BUILD_OUT)/makefile :
	mkdir -p $(HOST_BUILD_OUT);
	cd $(HOST_BUILD_OUT); $(ENV_CONFIGURE_BUILDHOST) $(BUILD_SRC)/runConfigureICU MacOSX --srcdir=$(BUILD_SRC) $(CONFIG_FLAGS) || ( echo "Error in runConfigureICU.  Contents of config.log:"; cat config.log; exit 1);
	# Manually copy Makefile.local over to the build-output directory (the old makefile
	# did this by copying ALL THE SOURCE over to the build output directory)
	cp $(BUILD_SRC)/common/Makefile.local $(HOST_BUILD_OUT)/common
	cp $(BUILD_SRC)/i18n/Makefile.local $(HOST_BUILD_OUT)/i18n


# To match the old makefile behavior, we also have to do some manual copying.  This is
# to make sure that the time-zone data we put into the ICU data file is the data drawn from our
# current SDK [or the system] and not the actual source data in our own source free.  For icuregions
# and icuzones, we just copy them to the right place in the build-output directory.  For metaZones,
# windowsZones, and timezoneTypes, we actually have to copy into the SOURCE directory, so we back up
# the original versions of those files, copy the ones from the SDK, build, and then restore the
# backups.  The copying and restoring of the time zone files is separated out into macros because
# we do the same thing in the "check" target below.  We also have to do extra gymnastics to save
# and report the error code after running ICU's makefile so that we restore the time zone files
# whether we succeed or fail.
# Also note that the original makefile actually checked to make sure the files we're copying
# out of $TZAUXFILESDIR actually exist before trying to copy-- I got rid of the check in an effort
# to simplify, but might need to put it back. --rtg 7/24/24

define doctor-tz-files
cp -p $(TZAUXFILESDIR)/icuregions $(HOST_BUILD_OUT)/tools/tzcode/
cp -p $(TZAUXFILESDIR)/icuzones $(HOST_BUILD_OUT)/tools/tzcode/
mkdir -p $(HOST_BUILD_OUT)/databkup/
cp -p $(BUILD_SRC)/data/misc/metaZones.txt $(HOST_BUILD_OUT)/databkup/
cp -p $(BUILD_SRC)/data/misc/timezoneTypes.txt $(HOST_BUILD_OUT)/databkup/
cp -p $(BUILD_SRC)/data/misc/windowsZones.txt $(HOST_BUILD_OUT)/databkup/
cp -p $(TZAUXFILESDIR)/metaZones.txt $(BUILD_SRC)/data/misc/
cp -p $(TZAUXFILESDIR)/timezoneTypes.txt $(BUILD_SRC)/data/misc/
cp -p $(TZAUXFILESDIR)/windowsZones.txt $(BUILD_SRC)/data/misc/
endef

define restore-tz-files
cp -p $(HOST_BUILD_OUT)/databkup/metaZones.txt $(BUILD_SRC)/data/misc/; \
cp -p $(HOST_BUILD_OUT)/databkup/timezoneTypes.txt $(BUILD_SRC)/data/misc/; \
cp -p $(HOST_BUILD_OUT)/databkup/windowsZones.txt $(BUILD_SRC)/data/misc/;
endef

icuhost : $(HOST_BUILD_OUT)/makefile
	$(doctor-tz-files)
	cd $(HOST_BUILD_OUT); \
		$(MAKE) $(MAKE_J) $(ENV_BUILDHOST) $(LIBOVERRIDES); \
		return_code=$$?; \
		$(restore-tz-files) \
		exit $$return_code

#################################
# tztools target
#################################

LIBICUTUX_OBJS = \
	$(BUILD_OUT)/tools/toolutil/collationinfo.o \
	$(BUILD_OUT)/tools/toolutil/filestrm.o \
	$(BUILD_OUT)/tools/toolutil/package.o \
	$(BUILD_OUT)/tools/toolutil/pkg_icu.o \
	$(BUILD_OUT)/tools/toolutil/pkgitems.o \
	$(BUILD_OUT)/tools/toolutil/swapimpl.o \
	$(BUILD_OUT)/tools/toolutil/toolutil.o \
	$(BUILD_OUT)/tools/toolutil/ucbuf.o \
	$(BUILD_OUT)/tools/toolutil/unewdata.o \
	$(BUILD_OUT)/tools/toolutil/uoptions.o \
	$(BUILD_OUT)/tools/toolutil/uparse.o \
	$(BUILD_OUT)/tools/toolutil/writesrc.o \
	$(BUILD_OUT)/common/*.o \
	$(BUILD_OUT)/stubdata/*.o

ICUZIC_SOURCES=$(BUILD_SRC)/tools/tzcode/zic.c $(BUILD_SRC)/tools/tzcode/localtime.c \
	$(BUILD_SRC)/tools/tzcode/asctime.c $(BUILD_SRC)/tools/tzcode/scheck.c $(BUILD_SRC)/tools/tzcode/ialloc.c

ICUGENRB_SOURCES = $(BUILD_SRC)/tools/genrb/errmsg.c $(BUILD_SRC)/tools/genrb/genrb.cpp \
	$(BUILD_SRC)/tools/genrb/parse.cpp $(BUILD_SRC)/tools/genrb/read.c $(BUILD_SRC)/tools/genrb/reslist.cpp \
	$(BUILD_SRC)/tools/genrb/ustr.c  $(BUILD_SRC)/tools/genrb/rbutil.c $(BUILD_SRC)/tools/genrb/wrtjava.cpp \
	$(BUILD_SRC)/tools/genrb/rle.c $(BUILD_SRC)/tools/genrb/wrtxml.cpp $(BUILD_SRC)/tools/genrb/prscmnts.cpp \
	$(BUILD_SRC)/tools/genrb/filterrb.cpp

$(BUILD_OUT)/libicutux.dylib :
	cd $(BUILD_OUT)/tools/toolutil; $(MAKE) $(ENV) $(LIBOVERRIDES);
	$($(ENV)) $(CXX) -current_version $(ICU_VERS).$(ICU_SUBVERS) -compatibility_version 1 -dynamiclib -dynamic \
		$(RC_ARCHS:%=-arch %) -g -Os -fno-exceptions -fvisibility=hidden -fvisibility-inlines-hidden $(ISYSROOT) \
		$(CXXFLAGS) $(LDFLAGS) \
		-install_name /usr/local/lib/libicutux.dylib -o $(BUILD_OUT)/libicutux.dylib \
		-L$(BUILD_OUT) -licucore $(LIBICUTUX_OBJS);

$(BUILD_OUT)/icuzic : $(BUILD_OUT)/libicutux.dylib $(ICUZIC_SOURCES)
	$($(ENV)) $(CC) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) -DSTD_INSPIRED \
		-dead_strip -o $(BUILD_OUT)/icuzic $(ICUZIC_SOURCES) \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/tools/toolutil \
		-L$(BUILD_OUT) -licutux

$(BUILD_OUT)/icugenrb : $(BUILD_OUT)/libicutux.dylib $(ICUGENRB_SOURCES)
	$($(ENV)) $(CXX) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) --std=c++17 \
		-dead_strip -o $(BUILD_OUT)/icugenrb $(ICUGENRB_SOURCES) \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/i18n -I$(BUILD_SRC)/tools/toolutil \
		-DICU_DATA_DIR -DU_SHOW_CPLUSPLUS_API -DU_OVERRIDE=override -DU_ALL_IMPLEMENTATION \
		-L$(BUILD_OUT) -licutux -licucore

$(BUILD_OUT)/icupkg : $(BUILD_OUT)/libicutux.dylib $(BUILD_SRC)/tools/icupkg/icupkg.cpp
	$($(ENV)) $(CXX) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) --std=c++17 \
		-dead_strip -o $(BUILD_OUT)/icupkg $(BUILD_SRC)/tools/icupkg/icupkg.cpp \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/tools/toolutil \
		-DICU_DATA_DIR -DU_SHOW_CPLUSPLUS_API -DU_OVERRIDE=override \
		-L$(BUILD_OUT) -licutux

$(BUILD_OUT)/tz2icu : $(BUILD_OUT)/libicutux.dylib $(BUILD_SRC)/tools/tzcode/tz2icu.cpp
	$($(ENV)) $(CXX) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) --std=c++17 \
		-dead_strip -o $(BUILD_OUT)/tz2icu $(BUILD_SRC)/tools/tzcode/tz2icu.cpp \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/tools/toolutil \
		-DU_SHOW_CPLUSPLUS_API -DU_OVERRIDE=override \
		-L$(BUILD_OUT) -licutux

$(BUILD_OUT)/icugenbrk : $(BUILD_OUT)/libicutux.dylib $(BUILD_SRC)/tools/genbrk/genbrk.cpp
	$($(ENV)) $(CXX) $(RC_ARCHS:%=-arch %) -g -Os $(ISYSROOT) --std=c++17 \
		-dead_strip -o $(BUILD_OUT)/icugenbrk $(BUILD_SRC)/tools/genbrk/genbrk.cpp \
		-I$(BUILD_SRC)/common -I$(BUILD_SRC)/tools/toolutil \
		-DICU_DATA_DIR -DU_SHOW_CPLUSPLUS_API -DU_OVERRIDE=override -DU_ALL_IMPLEMENTATION \
		-L$(BUILD_OUT) -licutux

tztools : $(BUILD_OUT)/libicucore.dylib $(BUILD_OUT)/libicutux.dylib $(BUILD_OUT)/icuzic \
	$(BUILD_OUT)/icugenrb $(BUILD_OUT)/icupkg $(BUILD_OUT)/tz2icu $(BUILD_OUT)/icugenbrk

#################################
# check target
# For now, this is implemented in the quick-and-dirty way, which is just to run the "make check"
# target in the icuhost directory.  This means we're just using the OSICU makefiles and not testing
# our own libicucore library, and it also means we still only support running the unit tests
# on the Mac, and not on the other platforms (or in the simulator).  I'm hoping to address this
# in the near future by converting the guts of this makefile into an Xcode project.
#################################
check : $(HOST_BUILD_OUT)/makefile
	rsync -a $(BUILD_SRC)/test/testdata/ $(HOST_BUILD_OUT)/test/testdata
	mkdir -p $(HOST_BUILD_OUT)/data/brkitr/rules
	cp $(BUILD_SRC)/data/brkitr/rules/word.txt $(HOST_BUILD_OUT)/data/brkitr/rules/word.txt
	$(doctor-tz-files)
	cd $(HOST_BUILD_OUT); \
		$(MAKE) $(ENV_BUILDHOST) ICU_DATA=$(HOST_BUILD_OUT)/data/out/$(DATA_FILE_NAME):$(HOST_BUILD_OUT)/test/testdata/out/testdata.dat $(LIBOVERRIDES) check; \
		return_code=$$?; \
		$(restore-tz-files) \
		exit $$return_code;

#################################
# icudata target
# Right now, this is just a shortcut to running the build-icu-data.sh script, but
# eventually, it might do more
#################################
icudata :
	apple/scripts/build-icu-data.sh
	
#################################
# installtests target
# This is for running the XCTests in ATP's CI
#################################
installtests :
	xcodebuild build-for-testing -project icu/icu4c/source/ICU.xcodeproj -scheme xc_icu_tests

