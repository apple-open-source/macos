# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

WIFI_FRAMEWORK=-framework CoreWLAN
ifeq ($(PLATFORM),iphoneos)
ARCHS=arm64 arm64e
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=x86_64 arm64e
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
endif

PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

test-wifiutil: WiFiUtil.m
	$(CC) $(PF_INC) -DTEST_WIFIUTIL -isysroot $(SYSROOT) $(ARCH_FLAGS) -framework CoreWiFi -framework Foundation -Wall -g -o $@ $^
	codesign -s - -f --entitlements entitlements-wifi.plist $@

clean:
	rm -f test-wifiutil *.o
	rm -rf *.dSYM

