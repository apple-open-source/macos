/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "test_base.h"

@implementation IPConfigurationFrameworkTestBase

@synthesize description;
@synthesize store;
@synthesize storeQueue;
@synthesize storeSem;
@synthesize interfaceController;
@synthesize interfaceQueue;
@synthesize interfaceSem;
@synthesize ifname;
@synthesize serviceKey;
@synthesize serviceSem;
@synthesize serviceKey2;
@synthesize serviceSem2;
@synthesize alternativeValidation;
@synthesize pdServiceKey;
@synthesize dhcpServerIfname;
@synthesize dhcpClientIfname;

+ (instancetype)sharedInstance
{
	static dispatch_once_t onceToken;
	static IPConfigurationFrameworkTestBase *instance;

	dispatch_once(&onceToken, ^{
		instance = [IPConfigurationFrameworkTestBase new];
	});

	return instance;
}

- (instancetype)init
{
	bool success = false;

	self = [super init];
	REQUIRE_NONNULL(self, done, "SETUP ERROR: Failed init");
	self.description = [NSString stringWithFormat:@"<%@ (%p)>", NSStringFromClass([self class]), self];
	REQUIRE_TRUE([self dynamicStoreInitialize], done, "SETUP ERROR: Failed to initialize dynamic store for test class instance");
	self.interfaceQueue = dispatch_queue_create(_LABEL "-interface", NULL);
	self.interfaceSem = dispatch_semaphore_create(0);
	REQUIRE_TRUE([self ioUserEthernetInterfaceCreate], done, "SETUP ERROR: Failed to initialize test interface for test class instance");
	self.serviceSem = dispatch_semaphore_create(0);
	self.serviceSem2 = dispatch_semaphore_create(0);
	self.alternativeValidation = FALSE;
	/* 
	 * This cleans up feth interfaces from previous test instances
	 * in case XCTest crashes/retries some test class.
	 * Assumes they were named feth0, feth1.
	 */
	destroyInterfaces(@"feth0", @"feth1");
	success = true;

done:
	if (!success) {
		NSLog(@"SETUP ERROR: Failed to initialize test base instance");
		exit(1);
	}
	return self;
}

- (void)dealloc
{
	[self dynamicStoreDestroy];
	[self ioUserEthernetInterfaceDestroy];
}

static __inline__ CFStringRef _Nonnull
copyDescriptionFunc(const void * info)
{
	__strong IPConfigurationFrameworkTestBase *strongSelf = (__bridge IPConfigurationFrameworkTestBase *)info;

	if (strongSelf != nil) {
		return CFStringCreateCopy(NULL, (__bridge CFStringRef)[strongSelf description]);
	} else {
		NSLog(@"Invalid object");
		return CFSTR("(null)");
	}
}

static __inline__ void
dynamicStoreContextMake(IPConfigurationFrameworkTestBase * base, SCDynamicStoreContext * retStoreContext)
{
	retStoreContext->version = 0;
	retStoreContext->info = (__bridge void *)base;
	retStoreContext->copyDescription = copyDescriptionFunc;
	retStoreContext->retain = CFRetain;
	retStoreContext->release = CFRelease;
}

static void
dynamicStoreCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void * info)
{
	__strong IPConfigurationFrameworkTestBase *strongSelf = (__bridge IPConfigurationFrameworkTestBase *)info;

	if (strongSelf == nil) {
		NSLog(@"Invalid object");
		return;
	}
	CFRetain(changedKeys);
	for (NSString *changedKey in (__bridge NSArray *)changedKeys) {
		@autoreleasepool {
			NSDictionary *changedVal = (__bridge_transfer NSDictionary *)SCDynamicStoreCopyValue(store, (__bridge CFStringRef)changedKey);

			if (changedVal == nil) {
				continue;
			}
			NSLogDebug(@"%s: SCDynamicStore changed key:\n%@ : %@", __func__, changedKey, changedVal);
			if ([changedKey containsString:@SCD_KEY_PLUGIN_IPCONFIGSERV]) {
				// An IPConfigurationService has been set, i.e. a change of pattern "Plugin:IPConfigurationService:[^\]+".
				// This notifies the changed service's semaphore so that the rest of the unit test can go on.
				if ([changedKey isEqualToString:[strongSelf serviceKey]]) {
					NSLogDebug(@"%s: Signaling semaphore for service '%@'", __func__, [strongSelf serviceKey]);
					(void)dispatch_semaphore_signal([strongSelf serviceSem]);
					break;
				} else if ([changedKey isEqualToString:[strongSelf serviceKey2]]) {
					NSLogDebug(@"%s: Signaling semaphore for other service '%@'", __func__, [strongSelf serviceKey2]);
					(void)dispatch_semaphore_signal([strongSelf serviceSem2]);
					break;
				}
			} else if ([changedKey containsString:@SCD_KEY_STATE_NETIF]) {
				NSString *netifName = [[changedKey componentsSeparatedByString:@"/"] objectAtIndex:3];

				if (![netifName isEqualToString:[strongSelf ifname]]) {
					continue;
				}
				// there's been a change to any key matching "State:/Network/Interface/tstX.*"
				if ([changedKey containsString:@"/IPConfigurationBusy"]) {
					// This means that an automatic IPConfigurationService has been set
					// Note: in a test with both v4 and v6, the v4 service is at self.service
					// and the v6 service is at self.service2. On default, the services in a
					// test function are assigned starting with self.service.
					if ([strongSelf serviceKey] != nil && [strongSelf alternativeValidation]) {
						NSLogDebug(@"%s: Signaling semaphore for service '%@'", __func__, [strongSelf serviceKey]);
						(void)dispatch_semaphore_signal([strongSelf serviceSem]);
						[strongSelf setAlternativeValidation:FALSE];
					}
				} else if ([changedKey containsString:@"/IPv6"]) {
					// This means that an AUTOMATIC-v6 service has been set
					if ([strongSelf serviceKey2] != nil && [strongSelf alternativeValidation]) {
						NSLogDebug(@"%s: Signaling semaphore for other service '%@'", __func__, [strongSelf serviceKey2]);
						(void)dispatch_semaphore_signal([strongSelf serviceSem2]);
						[strongSelf setAlternativeValidation:FALSE];
					}
				} else {
					// This is for keys ending in /Link or /LinkQuality
					static dispatch_once_t onceToken;

					dispatch_once(&onceToken, ^{
						NSLogDebug(@"%s: Signaling semaphore for interface '%@'", __func__, netifName);
						(void)dispatch_semaphore_signal([strongSelf storeSem]);
					});
				}
			}

		}
	}
	CFRelease(changedKeys);
}

- (BOOL)dynamicStoreInitialize
{
	BOOL success = FALSE;
	SCDynamicStoreContext storeContext = { 0 };

	[self setStoreSem:dispatch_semaphore_create(0)];
	REQUIRE_NONNULL([self storeSem], done, "Failed to create semaphore");
	dynamicStoreContextMake(self, &storeContext);
	[self setStore:SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR(_LABEL "-store"), dynamicStoreCallback, &storeContext)];
	REQUIRE_NONNULL([self store], done, "Failed to create SCDynamicStore session");
	success = SCDynamicStoreSetNotificationKeys([self store], NULL, SCD_KEY_PATTERNS_CFARRAYREF);
	REQUIRE_TRUE(success, done, "Failed to set notification keys with SCDynamicStore");
	[self setStoreQueue:dispatch_queue_create(_LABEL "-store", NULL)];
	REQUIRE_NONNULL([self storeQueue], done, "Failed to create a dispatch queue");
	success = SCDynamicStoreSetDispatchQueue([self store], [self storeQueue]);
	REQUIRE_TRUE(success, done, "Failed to set a dispatch queue for callback from SCDynamicStore");
	success = TRUE;

done:
	return success;
}

- (void)dynamicStoreDestroy
{
	if ([self store] != NULL) {
		SCDynamicStoreSetDispatchQueue(self.store, NULL);
		RELEASE_NULLSAFE(self.store);
	}
	[self setStoreQueue:nil];
	[self setStoreSem:nil];
}

static void
interfaceAttachedCallback(IOEthernetControllerRef controller, void * info)
{
	__strong IPConfigurationFrameworkTestBase *strongSelf = (__bridge IPConfigurationFrameworkTestBase *)info;
	io_object_t ioNetworkInterface = MACH_PORT_NULL;
	NSString *ifnameStr = NULL;
	bool success = false;

	if (strongSelf == nil) {
		NSLog(@"Invalid object");
		return;
	}
	REQUIRE_NONNULL(strongSelf, done, "Object is invalid");
	ioNetworkInterface = IOEthernetControllerGetIONetworkInterfaceObject(controller);
	REQUIRE_NEQUAL(ioNetworkInterface, MACH_PORT_NULL, done, "Failed to get io object for netif");
	ifnameStr = (__bridge_transfer NSString *)IORegistryEntryCreateCFProperty(ioNetworkInterface, CFSTR(kIOBSDNameKey), NULL, kNilOptions);
	REQUIRE_NONNULL(ifnameStr, done, "Failed to get interface name from attached io object properties");
	[strongSelf setIfname:ifnameStr];
	NSLogDebug(@"%s: Signaling semaphore for interface '%@'", __func__, [strongSelf ifname]);
	(void)dispatch_semaphore_signal([strongSelf interfaceSem]);
	success = true;

done:
	if (!success) {
		exit(1);
	}
	return;
}

- (BOOL)ioUserEthernetInterfaceCreate
{
	bool success = false;
	long error = 0;
	ether_addr_t fakeMACAddress = { .octet = { 0x02, 'T', 'E', 'S', 'T', 0x02 } };
	CFDataRef fakeMACAddressCFData = NULL;
	CFMutableDictionaryRef ethernetControllerProperties = NULL;
	CFMutableDictionaryRef ethernetControllerPropertiesKeyMerge = NULL;
	IOEthernetControllerRef ifController = NULL;
	__weak typeof(self) weakSelf = self;

#define ETH_CONTR_PROPS_KV_COUNT 4
	ethernetControllerProperties = CFDictionaryCreateMutable(kCFAllocatorDefault,
								 ETH_CONTR_PROPS_KV_COUNT,
								 &kCFTypeDictionaryKeyCallBacks,
								 &kCFTypeDictionaryValueCallBacks);
	REQUIRE_NONNULL(ethernetControllerProperties, done,
			"Failed to create dictionary for IOEthernetController properties");
	fakeMACAddressCFData = CFDataCreate(kCFAllocatorDefault, fakeMACAddress.octet, ETHER_ADDR_LEN);
	REQUIRE_NONNULL(fakeMACAddressCFData, done,
			"Failed to create MAC address CFData for IOEthernetController properties");
	CFDictionarySetValue(ethernetControllerProperties, kIOEthernetHardwareAddress, fakeMACAddressCFData);
	CFDictionarySetValue(ethernetControllerProperties, kIOUserEthernetInterfaceRole, CFSTR("test-ethernet"));
	CFDictionarySetValue(ethernetControllerProperties, CFSTR(kIOUserEthernetNamePrefix), CFSTR(NETIF_TEST_PREFIX));
#define ETH_CONTR_PROPS_MERGE_KV_COUNT 5
	ethernetControllerPropertiesKeyMerge = CFDictionaryCreateMutable(kCFAllocatorDefault,
									 ETH_CONTR_PROPS_MERGE_KV_COUNT,
									 &kCFTypeDictionaryKeyCallBacks,
									 &kCFTypeDictionaryValueCallBacks);
	REQUIRE_NONNULL(ethernetControllerPropertiesKeyMerge, done,
			"Failed to create merge dictionary for IOEthernetController properties");
	CFDictionarySetValue(ethernetControllerPropertiesKeyMerge, kSCNetworkInterfaceHiddenInterfaceKey, kCFBooleanTrue);
	CFDictionarySetValue(ethernetControllerPropertiesKeyMerge, CFSTR(kIOPropertyVendorNameKey), CFSTR("Apple"));
	CFDictionarySetValue(ethernetControllerPropertiesKeyMerge, CFSTR(kIOPropertyProductNameKey), CFSTR(NETIF_TEST_USERDEFINEDNAME));
	CFDictionarySetValue(ethernetControllerPropertiesKeyMerge, kSCNetworkInterfaceHiddenConfigurationKey, kCFBooleanTrue);
#define kIOIsEphemeralKey "IsEphemeral"
	CFDictionarySetValue(ethernetControllerPropertiesKeyMerge, CFSTR(kIOIsEphemeralKey), kCFBooleanTrue);
	REQUIRE_EQUAL(CFDictionaryGetCount(ethernetControllerPropertiesKeyMerge), ETH_CONTR_PROPS_MERGE_KV_COUNT, done,
		      "Failed to set all IOEthernetController merge key values");
	CFDictionarySetValue(ethernetControllerProperties, kIOUserEthernetInterfaceMergeProperties, ethernetControllerPropertiesKeyMerge);
	REQUIRE_EQUAL(CFDictionaryGetCount(ethernetControllerProperties), ETH_CONTR_PROPS_KV_COUNT, done,
		      "Failed to set all IOEthernetController top-level properties");
	ifController = IOEthernetControllerCreate(kCFAllocatorDefault, ethernetControllerProperties);
	REQUIRE_NONNULL(ifController, done, "Failed to create an IOEthernetController for new test ethernet interface");
	// waits for the test interface to come up
	IOEthernetControllerSetDispatchQueue(ifController, [self interfaceQueue]);
	IOEthernetControllerRegisterBSDAttachCallback(ifController, interfaceAttachedCallback, (__bridge void *)weakSelf);
	error = dispatch_semaphore_wait([self interfaceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	REQUIRE_NERROR(error, done, "Semaphore timed out waiting for new test interface to attach");
	// waits for the test interface network service key to show up in the SCDynamicStore
	error = dispatch_semaphore_wait([self storeSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	REQUIRE_NERROR(error, done, "Semaphore timed out waiting for new test interface to show up in store");
	[self setInterfaceController:ifController];
	REQUIRE_EQUAL(IOEthernetControllerSetLinkStatus(ifController, TRUE), kIOReturnSuccess, done, "Failed to set link status up on new interface");
	success = true;

done:
	RELEASE_NULLSAFE(fakeMACAddressCFData);
	RELEASE_NULLSAFE(ethernetControllerPropertiesKeyMerge);
	RELEASE_NULLSAFE(ethernetControllerProperties);
	return success;
}

- (void)ioUserEthernetInterfaceDestroy
{
	IOEthernetControllerSetLinkStatus(self.interfaceController, FALSE);
	CFRelease(self.interfaceController);
	[self setInterfaceQueue:nil];
	[self setInterfaceSem:nil];
}

- (void)setService:(IPConfigurationServiceRef)service
{
	if (service == NULL) {
		self.serviceKey = nil;
	} else {
		self.serviceKey = (__bridge NSString *)IPConfigurationServiceGetNotificationKey(service);
	}
}

- (void)setService2:(IPConfigurationServiceRef)service
{
	if (service == NULL) {
		self.serviceKey2 = nil;
	} else {
		self.serviceKey2 = (__bridge NSString *)IPConfigurationServiceGetNotificationKey(service);
	}
}

@end
