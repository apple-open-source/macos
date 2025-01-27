# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
ARCHS=arm64 arm64e
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=arm64 x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=arm64 x86_64
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
IPF = -I../IPConfiguration_framework
SC_PRIV = -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

test-iphpvd: IPHPvDInfoRequestServer.m ../bootplib/DNSNameList.c ../bootplib/util.c ../bootplib/IPConfigurationLog.c ../IPConfiguration_framework/IPConfigurationPrivate.c ../bootplib/cfutil.c
	$(CC) -Wall -g -isysroot $(SYSROOT) $(ARCH_FLAGS) -DTESTHARNESS_IPHPVD -DDEBUG  $(PF_INC) $(SYSTEM_PRIVATE) $(BOOTPLIB) $(IPF) $(SC_PRIV) -framework CoreFoundation -framework CFNetwork -framework SystemConfiguration -o $@ $^

clean:
	rm -f test-iphpvd
	rm -rf *.dSYM/
