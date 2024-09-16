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

@interface IPv4ManualConfigTest : XCTestCase
@property (nonatomic) IPConfigurationFrameworkTestBase * base;
@end

@implementation IPv4ManualConfigTest

@synthesize base;

- (void)setUp 
{
	[self setBase:[IPConfigurationFrameworkTestBase sharedInstance]];
}

- (void)tearDown 
{
	// pass
}

#pragma mark -
#pragma mark Manual IPv4

- (void)testNegativeIPv4ManualAddressMissingMethod
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR]
			// bad configuration, needs kSCValNetIPv4ConfigMethodManual
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert bad configuration for IPv4 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4ManualMethodMissingAddress
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual)
			// bad configuration, needs kSCPropNetIPv4Addresses
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert bad configuration for IPv4 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4ManualIncompleteOptions
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@MANUAL_IPV4_SUBNET_MASK],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4DestAddresses) : @[@MANUAL_IPV4_ADDR],
#define MANUAL_IPV4_ROUTER_ADDR "10.0.0.1"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Router) : @MANUAL_IPV4_ROUTER_ADDR
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert bad configuration for IPv4 service");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4ManualInvalidExtraOptions
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR],
#define EXTRA_PROP_IPV6_PREFIX_LEN @[@7]
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : EXTRA_PROP_IPV6_PREFIX_LEN
			// will hit the "extra props" code path because everything else is correct
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert bad property for IPv4 configuration");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4ManualOneInvalidAddress
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
#define BAD_IPV4_ADDR "256.0.0.0"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@BAD_IPV4_ADDR]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to get NULL service for manual IPv4 service with bad IPv4 addr");

done:
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4ManualValidAddressBadSubnetMask
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR],
#define BAD_IPV4_SUBNET_MASK "-1.0.0.0"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@BAD_IPV4_SUBNET_MASK]
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

- (void)testIPv4ManualOneValidAddressOneValidSubnetMask
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@MANUAL_IPV4_SUBNET_MASK]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv4 service with private addr");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));

	// subscribes on SCDynamicStore for notifications in the service key
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");

	// gets service properties from the SCDynamicStore
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);

	// validates that the configuration that we set at the top is the configuration we get back (ignoring some details)
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)],
			      "Failed to assert manually assigned IPv4 address");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testIPv4ManualNAddressesNSubnetMasks
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *customServiceProperties = nil;
#define N_ADDRS 10
	NSMutableArray *customServiceAddrsArray = [NSMutableArray arrayWithCapacity:N_ADDRS];
	NSMutableArray *customServiceSubnetMasksArray = [NSMutableArray arrayWithCapacity:N_ADDRS];

#define N_ADDRS_MAX 254
#define IPV4_ADDR_PREFIX "10.0.0"
#define IPV4_ADDR_SUFFIX(i) ((i)+2)
	// creates N_ADDRS subsequent addresses starting from 10.0.0.2
	for (size_t i = 0; i < N_ADDRS && IPV4_ADDR_SUFFIX(i) < N_ADDRS_MAX; i++) {
		[customServiceAddrsArray addObject:[NSString stringWithFormat:@"%s.%zu", IPV4_ADDR_PREFIX, IPV4_ADDR_SUFFIX(i)]];
		[customServiceSubnetMasksArray addObject:@MANUAL_IPV4_SUBNET_MASK];
	}
	customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : (NSArray *)customServiceAddrsArray,
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : (NSArray *)customServiceSubnetMasksArray
		}
	};

	// creates and starts the configuration service
	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@'\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create IPv4 configuration service");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);

	// validates the type of the returned service properties
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done,
			    "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)), done,
			    "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);

	// validates that the configuration that we set at the top is the configuration we get back (ignoring some details)
	// the IPConfiguration server must configure the interface with just the first viable address in the provided list
	XCTAssertEqualObjects([customServiceAddrsArray objectAtIndex:0],
			      [[[servicePropertiesFromStore
				 objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)]
			       objectAtIndex:0],
			      "Failed to assert manually assigned IPv4 addresses");
	XCTAssertEqualObjects([customServiceSubnetMasksArray objectAtIndex:0],
			      [[[servicePropertiesFromStore
				 objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
				objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)]
			       objectAtIndex:0],
			      "Failed to assert manually assigned subnet masks");
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testIPv4ManualTwiceWithSameProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	IPConfigurationServiceRef service2 = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *servicePropertiesFromStore2 = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv4 service with private addr");
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
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");

	// validates that the creation of a second manual service with the same parameters succeeds and is updated in the store
	service2 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service2), done, "Failed to create manual IPv4 service with private addr");
	[[self base] setService2:service2];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service2));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service2), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore2 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service2);
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore2, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore2)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore2);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");

done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service);
	RELEASE_NULLSAFE(service2);
}


- (void)testIPv4ManualTwiceWithDifferentProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	IPConfigurationServiceRef service2 = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *servicePropertiesFromStore2 = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR]
		}
	};
	NSDictionary *customServiceProperties2 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
#define NEW_MANUAL_IPV4_ADDR "10.0.0.3"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@NEW_MANUAL_IPV4_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@MANUAL_IPV4_SUBNET_MASK]

		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create manual IPv4 service with private addr");
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
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");

	// validates that the creation of a second manual service with different parameters succeeds and is updated in the store
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties2);
	service2 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties2);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service2), done, "Failed to create manual IPv4 service with private addr");
	[[self base] setService2:service2];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service2));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service2), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore2 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service2);
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore2, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore2)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore2);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties2
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore2
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");

done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service);
	RELEASE_NULLSAFE(service2);
}

@end
