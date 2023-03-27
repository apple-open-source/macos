# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

WIFI_FRAMEWORK=-framework CoreWLAN -framework Apple80211
ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
#ARCHS=armv7
WIFI_FRAMEWORK=-framework MobileWiFi -lio80211
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
#ARCHS=i386 x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

w: wireless.m
	cc -DTEST_WIRELESS_M -I../EAP8021X.fproj -g -o $@ $^ $(PF_INC) -framework CoreWLAN -framework Foundation

wireless: wireless.c wireless.m
	$(CC) $(ARCH_FLAGS) -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS -I../EAP8021X.fproj -DTEST_WIRELESS -g -o $@ $^ $(PF_INC) $(WIFI_FRAMEWORK) -framework Foundation -framework EAP8021X -framework SystemConfiguration

wireless_p: wireless.c
	$(CC) $(ARCH_FLAGS) -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS -I../EAP8021X.fproj -DTEST_WIRELESS -g -o $@ $^ $(PF_INC) $(WIFI_FRAMEWORK) -framework Foundation -framework EAP8021X -framework SystemConfiguration
	codesign -s - -f --entitlements entitlements.plist $@

Dialogue: Dialogue.c
	cc -Wall -DTEST_DIALOGUE -g -o Dialogue Dialogue.c mylog.c ../EAP8021X.fproj/EAPLog.c -I../EAP8021X.fproj ../EAP8021X.fproj/myCFUtil.c -framework CoreFoundation -framework SystemConfiguration -framework EAP8021X -F/System/Library/PrivateFrameworks -framework Security

InterestNotification: InterestNotification.c ../EAP8021X.fproj/EAPLog.c
	$(CC) -DTEST_INTERESTNOTIFICATION -I../EAP8021x.fproj -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) -framework SystemConfiguration -framework CoreFoundation -framework IOKit $(WARNING_CFLAGS) -g -o $@ $^
	codesign -s - $@

clean:
	rm -f Dialogue wireless_p wireless InterestNotification
	rm -rf *.dSYM/
