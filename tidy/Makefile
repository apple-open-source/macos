##
# Makefile for tidy
##


# Project info
Project               = tidy
BuildNumber           = $(RC_ProjectSourceVersion)
ifeq ($(BuildNumber),)
BuildNumber           = 16
endif
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
OTHER_LDFLAGS=-Wl,-iosmac_version_min,12.0
SECTORDER_FLAGS=-sectorder __TEXT __text $(SDK_DIR)/usr/local/lib/OrderFiles/libtidy.order
endif

ifdef TIDY_DEBUG
CFLAGS= -O0
endif

ifneq ($(SDK_DIR),/)
CFLAGS += -isysroot $(SDK_DIR)
endif

CFLAGS+= -DTIDY_APPLE_CHANGES=1 -DTIDY_APPLE_BUILD_NUMBER=$(BuildNumber) -DTIDY_APPLE_BUILD_NUMBER_STR='"\"$(BuildNumber)\""'

# Additional warning flags. See tidy/build/gmake/Makefile for others.
CFLAGS+= -Wformat=2 -Wmissing-format-attribute

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

# Ensure that our make subinvocations work with commands from the apppropriate toolchain.
export AR
export CC
export RANLIB
export STRIP

install::
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installhdrs devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installib devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
	$(CC) $(CFLAGS) $(OTHER_LDFLAGS) -dynamiclib $(SECTORDER_FLAGS) -o "$(DSTROOT)/usr/lib/libtidy.A.dylib" -Wl,-force_load,"$(OBJROOT)/tidy/lib/libtidy.a" -install_name "/usr/lib/libtidy.A.dylib" -compatibility_version 1.0.0 -current_version 1.0.0
	$(LN) -s "libtidy.A.dylib" "$(DSTROOT)/usr/lib/libtidy.dylib"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installexes devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr" LIBDIR="$(DSTROOT)/usr/lib"
	TIDY_APPLE_CHANGES=1 CFLAGS="$(CFLAGS) -fno-common" runinst_prefix="$(DSTROOT)/usr" devinst_prefix="$(DSTROOT)/usr" $(MAKE) -C "$(OBJROOT)/$(Project)/build/gmake" installmanpage_apple devinst_prefix="$(DSTROOT)/usr" runinst_prefix="$(DSTROOT)/usr"
ifndef TIDY_DEBUG
	$(STRIP) "$(DSTROOT)/usr/bin/tab2space"
	$(STRIP) "$(DSTROOT)/usr/bin/tidy"
	$(STRIP) -x "$(DSTROOT)/usr/lib/libtidy.A.dylib"
endif
	$(RM) "$(DSTROOT)/usr/lib/libtidy.a"

	$(MKDIR) "$(DSTROOT)/usr/include/$(Project)"
	$(INSTALL_FILE) "$(SRCROOT)/$(Project).modulemap" "$(DSTROOT)/usr/include/$(Project)/module.modulemap"

	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt
