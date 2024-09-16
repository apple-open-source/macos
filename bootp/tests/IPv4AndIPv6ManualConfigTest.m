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

@interface IPv4AndIPv6ManualConfigTest : XCTestCase
@property (nonatomic) IPConfigurationFrameworkTestBase * base;
@end

@implementation IPv4AndIPv6ManualConfigTest

- (void)setUp 
{
	[self setBase:[IPConfigurationFrameworkTestBase sharedInstance]];
}

- (void)tearDown 
{
	// pass
}


#pragma mark -
#pragma mark Manual IPv4 and IPv6

- (void)testNegativeIPv4AndIPv6NullProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Creating IPConfigurationService with NULL properties");
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, NULL);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create service with NULL props");
	[[self base] setService:service];
	[[self base] setAlternativeValidation:TRUE];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], NULL, SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCTAssertEqual(err, 0, "Semaphore timed out waiting for service to be up");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4AndIPv6EmptyProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Creating IPConfigurationService with NULL properties");
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create service with NULL props");
	[[self base] setService:service];
	[[self base] setAlternativeValidation:TRUE];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], NULL, SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCTAssertEqual(err, 0, "Semaphore timed out waiting for service to be up");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testNegativeIPv4AndIPv6InvalidProperties
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{@"key":@"val"};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Creating IPConfigurationService with NULL properties");
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create service with NULL props");
	[[self base] setService:service];
	[[self base] setAlternativeValidation:TRUE];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], NULL, SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCTAssertEqual(err, 0, "Semaphore timed out waiting for service to be up");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

- (void)testIPv4AndIPv6IPServicesManual
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service4 = NULL;
	IPConfigurationServiceRef service6 = NULL;
	NSDictionary *servicePropertiesFromStore4 = nil;
	NSDictionary *servicePropertiesFromStore6 = nil;
	NSDictionary *customServiceProperties4 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses) : @[@MANUAL_IPV4_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks) : @[@MANUAL_IPV4_SUBNET_MASK]
		}
	};
	NSDictionary *customServiceProperties6 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodManual),
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses) : @[@MANUAL_IPV6_ADDR],
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength) : @[@MANUAL_IPV6_PREFIXLEN]
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");

	// ipv4
	NSLogDebug(@"Custom properties for new IPv4 IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties4);
	service4 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties4);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service4), done, "Failed to create manual ipv4 service");
	[[self base] setService:service4];
	NSLogDebug(@"Successfully instantiated IPv4 IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service4));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service4), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore4 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service4);

	// validates the ipv4 configuration
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore4, done, "Failed to retrieve IPv4 IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore4)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPv4 IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore4);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore4
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties4
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      [[servicePropertiesFromStore4
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)],
			      "Failed to assert manually assigned IPv4 address");
	XCTAssertEqualObjects([[customServiceProperties4
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)],
			      [[servicePropertiesFromStore4
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)],
			      "Failed to assert manually assigned IPv4 addr prefix");

	// ipv6
	NSLogDebug(@"Custom properties for new IPv6 IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties6);
	service6 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties6);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service6), done, "Failed to create ipv6 service");
	[[self base] setService2:service6];
	NSLogDebug(@"Successfully instantiated IPv6 IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service6));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service6), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore6 = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service6);

	// validates the ipv6 configuration
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore6, done, "Failed to retrieve active IPv6 IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore6)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPv6 IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore6);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore6
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertEqualObjects([[customServiceProperties6
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      [[servicePropertiesFromStore6
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6Addresses)],
			      "Failed to assert manually assigned IPv6 address");
	XCTAssertEqualObjects([[customServiceProperties6
				objectForKey:BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      [[servicePropertiesFromStore6
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv6)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv6PrefixLength)],
			      "Failed to assert manually assigned IPv6 addr prefix");

done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service4);
	RELEASE_NULLSAFE(service6);
}

@end
