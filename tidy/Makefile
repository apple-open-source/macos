##
# Makefile for tidy
##


# Project info
Project               = tidy
BuildNumber           = 15.15
UserType              = Administrator
ToolType              = Libraries

ifeq "$(RC_TARGET_CONFIG)" ""
RC_TARGET_CONFIG = MacOSX
endif

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

#Install_Target = install-strip
lazy_install_source:: shadow_source

export SDKROOT ?= /
ifeq "$(SDKROOT)" "/"
export SDK_DIR = /
else
export SDK_DIR = $(shell xcodebuild -version -sdk $(SDKROOT) Path)
endif

ifeq "$(RC_TARGET_CONFIG)" "MacOSX"
SECTORDER_FLAGS=-sectorder __TEXT __text $(SDK_DIR)/usr/local/lib/OrderFiles/libtidy.order
endif

ifdef TIDY_DEBUG
CFLAGS= -O0
endif

ifneq ($(SDK_DIR),/)
CFLAGS += -isysroot $(SDK_DIR)
endif

CFLAGS+= -DTIDY_APPLE_CHANGES=1 -DTIDY_APPLE_BUILD_NUMBER=$(BuildNumber) -DTIDY_APPLE_BUILD_NUMBER_STR='"\"$(BuildNumber)\""'

# seriously gross B&I hackery
# blame molson
ifeq "$(RC_ProjectName)" "tidy_Sim"
DEVELOPER_DIR ?= $(shell xcode-select -print-path)
include  $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.indigo
ActualDSTROOT = ${DSTROOT}/${INDIGO_PREFIX}
else
ActualDSTROOT = ${DSTROOT}
endif

OSV     = $(ActualDSTROOT)/usr/local/OpenSourceVersions
OSL     = $(ActualDSTROOT)/usr/local/OpenSourceLicenses

# Ensure that our make subinvocations work with commands from the apppropriate toolchain.
export AR
export CC
export RANLIB
export STRIP

install::
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActuallDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installhdrs devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installib devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
	$(CC) $(CFLAGS) -dynamiclib $(SECTORDER_FLAGS) -o "$(ActualDSTROOT)/usr/lib/libtidy.A.dylib" "$(OBJROOT)/tidy/lib/libtidy.a" -install_name "/usr/lib/libtidy.A.dylib" -all_load -compatibility_version 1.0.0 -current_version 1.0.0
	$(LN) -s "libtidy.A.dylib" "$(ActualDSTROOT)/usr/lib/libtidy.dylib"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installexes devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr" LIBDIR="$(ActualDSTROOT)/usr/lib"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(ActualDSTROOT)/usr" devinst_prefix="$(ActualDSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installmanpage_apple devinst_prefix="$(ActualDSTROOT)/usr" runinst_prefix="$(ActualDSTROOT)/usr"
ifndef TIDY_DEBUG
	$(STRIP) "$(ActualDSTROOT)/usr/bin/tab2space"
	$(STRIP) "$(ActualDSTROOT)/usr/bin/tidy"
	$(STRIP) -x "$(ActualDSTROOT)/usr/lib/libtidy.A.dylib"
endif
	$(RM) "$(ActualDSTROOT)/usr/lib/libtidy.a"

	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
