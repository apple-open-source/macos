# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),)
PLATFORM=macosx
endif

SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc -fno-color-diagnostics

#PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

DNSINFO_FILES = dnsinfo_create.c ../libSystemConfiguration/libSystemConfiguration_client.c
DNSINFO_INCLUDES = -I../Plugins/IPMonitor -I../nwi -I../libSystemConfiguration -I../SystemConfiguration.fproj
dnsinfo_flatfile_test: dnsinfo_flatfile.c $(DNSINFO_FILES)
	$(CC) -DTEST_DNSINFO_FLATFILE $(DNSINFO_INCLUDES) -isysroot $(SYSROOT) -framework SystemConfiguration -framework CoreFoundation -Wall -g -o $@ $^
	codesign -s - $@

