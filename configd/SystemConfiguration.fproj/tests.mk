# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

WIFI_FRAMEWORK=-framework CoreWLAN
ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
CORETELEPHONY=
endif

ifeq ($(PLATFORM),watchos)
# watchOS internal SDK
CORETELEPHONY=-framework CoreTelephony
WIFI_FRAMEWORK=-framework MobileWiFi 
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
#ARCHS=x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc -fno-color-diagnostics
endif

PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))
SCPRIV=-DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

SYSPRIV=-I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders
SC_INCLUDE=-I. -I../IPMonitorControl -I./helper -I../Plugins/common

SCNETWORKINTERFACE_TEST_CFILES = SCNetworkInterface.c ../IPMonitorControl/IPMonitorControl.c VLANConfiguration.c BridgeConfiguration.c SCNetworkConfigurationInternal.c SCNetworkService.c SCNetworkProtocol.c LinkConfiguration.c

SCNETWORK_CATEGORY_MANAGER_TEST_CFILES = SCNetworkCategoryManager.c ../CategoryManager/CategoryManager.c

SCNETWORK_DEFAULT_SET_TEST_CFILES = SCNetworkSet.c SCNetworkInterface.c ../IPMonitorControl/IPMonitorControl.c VLANConfiguration.c BridgeConfiguration.c SCNetworkConfigurationInternal.c SCNetworkService.c SCNetworkProtocol.c LinkConfiguration.c SCDPrivate.c

ifeq ($(PLATFORM),macosx)
SCNETWORKINTERFACE_TEST_CFILES += BondConfiguration.c
SCNETWORK_DEFAULT_SET_TEST_CFILES += BondConfiguration.c
endif

scnetworkinterface: $(SCNETWORKINTERFACE_TEST_CFILES)
	$(CC) -DTEST_SCNETWORKINTERFACE -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework Foundation -framework SystemConfiguration -framework IOKit $(PF_INC) -lCrashReporterClient -Wall -g -o $@ $^
	codesign -s - $@

scnetworkcategory: $(SCNETWORK_CATEGORY_MANAGER_TEST_CFILES)
	$(CC) -DTEST_SCNETWORK_CATEGORY_MANAGER -I../common -I../CategoryManager -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework Foundation -framework SystemConfiguration $(PF_INC) -Wall -g -o $@ $^
	codesign -s - --entitlements category-manager-entitlements.plist $@

category-transform: SCNetworkCategory.c
	$(CC) -DTEST_TRANSFORM -I../common -I../CategoryManager -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework Foundation -framework SystemConfiguration $(PF_INC) -Wall -g -o $@ $^
	codesign -s - $@

network-settings: SCNetworkSettingsManager.c $(SCNETWORK_CATEGORY_MANAGER_TEST_CFILES)
	$(CC) -DTEST_SCNETWORK_SETTINGS -I../common -I../CategoryManager -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework Foundation -framework SystemConfiguration $(PF_INC) -Wall -g -o $@ $^
	codesign -s - --entitlements category-observer-entitlements.plist $@

scnetworkdefaultset: $(SCNETWORK_DEFAULT_SET_TEST_CFILES)
	$(CC) -DTEST_SCNETWORK_DEFAULT_SET -I../common -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework IOKit -framework Foundation -framework SystemConfiguration -lCrashReporterClient $(PF_INC) -Wall -g -o $@ $^
	codesign -s - $@

sccontrolprefs: SCControlPrefs.c
	$(CC) -DTEST_SCCONTROL_PREFS -I../common -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework SystemConfiguration $(PF_INC) -framework CoreFoundation -Wall -g -o $@ $^
	codesign -s - $@

valid-dns-names: SCValidation.c
	$(CC) -DTEST_VALID_DNS_NAMES -I../common -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSPRIV) $(SC_INCLUDE) -framework SystemConfiguration $(PF_INC) -framework CoreFoundation -Wall -g -o $@ $^
	codesign -s - $@

clean:
	rm -rf *.o *~ *.dSYM scnetworkdefaultset network-settings category-transform scnetworkcategory scnetworkinterface sccontrolprefs valid-dns-names
