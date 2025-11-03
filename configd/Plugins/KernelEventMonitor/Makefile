ifeq ($(PLATFORM),)
PLATFORM=macosx
endif

# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) clang -fno-color-diagnostics
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

EXTRA_CFLAGS=-fsanitize=address
#EXTRA_CFLAGS=

TEST_INCLUDE=-I. -I../../common -I./Control -I../common -I../../dnsinfo -I../../nwi -I../../libSystemConfiguration -I../../SystemConfiguration.fproj -I../../IPMonitorControl -I../../IPMonitorControl/AWD -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

# ----------

interface-expensive: ev_extra.c ../common/WiFiUtil.m
	$(CC) -DTEST_INTERFACE_EXPENSIVE $(TEST_INCLUDE) $(PF_INC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -framework CoreFoundation -framework SystemConfiguration -framework CoreWiFi -Wall -O0 -g -o $@ $^
	codesign -s - $@
