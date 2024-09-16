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

#import <XCTest/XCTest.h>

#import "test_base.h"

@interface IPv6ManualConfigTest : XCTestCase
@property (nonatomic) IPConfigurationFrameworkTestBase * base;
@end

@implementation IPv6ManualConfigTest

- (void)setUp 
{
	[self setBase:[IPConfigurationFrameworkTestBase sharedInstance]];
}

- (void)tearDown 
{
	// pass
}

#pragma mark -
#pragma mark Manual IPv6

- (void)testNegativeIPv6ManualMissingMethod
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN]
			// bad configuration, needs kSCValNetIPv6ConfigMethodManual
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert NULL IPv6 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv6ManualOneAddressMissingPrefixLength
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR]
			// bad configuration, no prefix length for given address
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert NULL IPv6 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv6ManualWithInvalidExtraProp
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN],
#define IPV6_PROP_EXTRA "/16"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@IPV6_PROP_EXTRA]
			// will hit the "extra props" code path because all other props are correct
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert NULL IPv6 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv6ManualOneValidAddressManyPrefixesInvalidPrefixProps
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
#define IPV6_PREFIX_EXTRA 8
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN, @IPV6_PREFIX_EXTRA]
			// bad configuration, 2 prefixes but only 1 addr
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert NULL IPv6 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testIPv6ManualOneValidAddressOneValidPrefixAdditionalOptions
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionPerformNUD) : [NSNumber numberWithBool:TRUE],
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionEnableDAD) : [NSNumber numberWithBool:TRUE],
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionEnableCLAT46) : [NSNumber numberWithBool:TRUE],
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv6 service with unique local addr");
	[[self base] setService:service];
	[[self base] setServiceSem:dispatch_semaphore_create(0)];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testIPv6ManualWithNAddressesAndNPrefixLengths
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *customServiceProperties = nil;
#define N_ADDRS 10
	NSMutableArray *customServiceAddrsArray = [NSMutableArray arrayWithCapacity:N_ADDRS];
	NSMutableArray *customServicePrefixLengthsArray = [NSMutableArray arrayWithCapacity:N_ADDRS];

#define N_ADDRS_MAX 0xfffe
#define IPV6_ADDR_PREFIX "fc00"
#define IPV6_PREFIX_LEN_MAX 128
#define IPV6_PREFIX_LEN(i) ((i)+7)
#define IPV6_ADDR_SUFFIX(i) ((i)+2)
	// creates N_ADDRS subsequent addresses starting from fc00::2
	for (size_t i = 0; i < N_ADDRS && IPV6_ADDR_SUFFIX(i) < N_ADDRS_MAX && IPV6_PREFIX_LEN(i) < IPV6_PREFIX_LEN_MAX; i++) {
		[customServiceAddrsArray addObject:[NSString stringWithFormat:@"%s::%zx", IPV6_ADDR_PREFIX, IPV6_ADDR_SUFFIX(i)]];
		[customServicePrefixLengthsArray addObject:@IPV6_PREFIX_LEN(i)];
	}
	customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : (NSArray *)customServiceAddrsArray,
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : (NSArray *)customServicePrefixLengthsArray
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv6 service with unique local addr");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	// validates that the active service is using the first address only
	XCTAssertEqualObjects([[[customServiceProperties
				 objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)]
			       objectAtIndex:0],
			      [[[servicePropertiesFromStore
				 objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)]
			       objectAtIndex:0],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[[customServiceProperties
				 objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)]
			       objectAtIndex:0],
			      [[[servicePropertiesFromStore
				 objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)]
			       objectAtIndex:0],
			      "Failed to assert manually assigned IPv6 addr prefix");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testIPv6ManualTwiceWithSameProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	IPConfigurationServiceRef service2 = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *servicePropertiesFromStore2 = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv6 service with unique local addr");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);
	// validates the active manual configuration
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

	// validates that a new service gets created and published, although old service (duplicate) is still there
	NSLogDebug(@"Custom properties for replacement IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service2 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service2), done, "Failed to create replacement IPv6 service with unique local addr");
	[[self base] setService2:service2];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service2));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service2), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC_SHORT));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore2 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service2);

	// validates that both services exist on the store and are the same
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore2, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore2)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore2);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service);
	RELEASE_NULLSAFE(service2);
}


- (void)testIPv6ManualTwiceWithDifferentProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	IPConfigurationServiceRef service2 = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *servicePropertiesFromStore2 = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN]
		}
	};
	NSDictionary *customServiceProperties2 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
#define IPV6_ADDR2 "fc00::3"
#define IPV6_PREFIXLEN2 64
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@IPV6_ADDR2],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@IPV6_PREFIXLEN2]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv6 service with unique local addr");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);

	// validates the active manual configuration
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties2);
	service2 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties2);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service2), done, "Failed to create replacement IPv6 service with unique local addr");
	[[self base] setService2:service2];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service2));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service2), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys,
			 done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore2 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service2);

	// validates the second manual configuration
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore2, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore2)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore2);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties2
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[customServiceProperties2
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service);
	RELEASE_NULLSAFE(service2);
}

@end
