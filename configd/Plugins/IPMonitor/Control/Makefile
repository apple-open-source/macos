ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
ARCHS=arm64
endif

ifeq ($(PLATFORM),)
PLATFORM=macosx
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=x86_64
endif

# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) clang
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

EXTRA_CFLAGS=-fsanitize=address
#EXTRA_CFLAGS=

TEST_INCLUDE=-I. -I../../../SystemConfiguration.fproj -I../common -I../../dnsinfo -I../../nwi -I../../libSystemConfiguration -I../../IPMonitorControl -I../../IPMonitorControl/AWD -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

test-IPMonitorControlPrefs: IPMonitorControlPrefs.c
	$(CC) -DTEST_IPMONITORCONTROLPREFS $(TEST_INCLUDE) $(PF_INC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -lnetwork -framework CoreFoundation -framework SystemConfiguration -Wall -O0 -g -o $@ $^


