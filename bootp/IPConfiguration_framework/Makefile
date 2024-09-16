# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
ARCHS=arm64 arm64e
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=arm64e x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=arm64e x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc -fno-color-diagnostics -fobjc-arc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))
SYSTEM_PRIVATE = -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
BOOTPLIB = -I../bootplib
IPH = -I../IPConfigurationHelper
#SC_PRIV = -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

test-pvd-info-request: PvDInfoRequest.m ObjectWrapper.c IPConfigurationPrivate.c ../bootplib/IPConfigurationLog.c ../bootplib/cfutil.c
	$(CC) -Wall -g -isysroot $(SYSROOT) $(ARCH_FLAGS) -D__TEST_PVDINFOREQUEST_SPI__ -DDEBUG $(BOOTPLIB) $(IPH) -framework CoreFoundation -framework SystemConfiguration -o $@ $^
	codesign -s - -f --entitlements test-entitlements.plist $@

clean:
	rm -f test-pvd-info-request
	rm -rf *.dSYM/
