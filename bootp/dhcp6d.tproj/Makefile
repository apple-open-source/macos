# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

WIFI_FRAMEWORK=-framework CoreWLAN
ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=arm64
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
CORETELEPHONY=
endif

ifeq ($(PLATFORM),watchos)
# watchOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=armv7k
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))
SCPRIV=-DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

SYSPRIV=-I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

DHCPv6Server: main.c DHCPv6Server.c ../bootplib/ptrlist.c ../bootplib/DHCPv6.c ../bootplib/DHCPv6Options.c ../bootplib/DNSNameList.c ../bootplib/DHCPDUID.c ../bootplib/cfutil.c ../bootplib/util.c ../bootplib/IPv6Socket.c ../bootplib/interfaces.c ../bootplib/dynarray.c ../bootplib/IPConfigurationLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) $(SYSPRIV) -D__APPLE_USE_RFC_3542 -I../bootplib -Wall -framework CoreFoundation -framework SystemConfiguration -g -o $@  $^

clean:
	rm -rf *~ *.dSYM DHCPv6Server
