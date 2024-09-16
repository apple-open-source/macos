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

@interface IPv4AutomaticConfigTest : XCTestCase
@property (nonatomic) IPConfigurationFrameworkTestBase * base;
@end

@implementation IPv4AutomaticConfigTest

- (void)setUp 
{
	[self setBase:[IPConfigurationFrameworkTestBase sharedInstance]];
}

- (void)tearDown 
{
	// pass
}

#pragma mark -
#pragma mark IPv4 Automatic

- (void)testNegativeIPv4MethodAutomaticFailsWithInvalidOption
{
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodAutomatic)
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNil(BRIDGED_ID(service), "Failed to assert invalid configuration option");

done:
	RELEASE_NULLSAFE(service);
}


#pragma mark -
#pragma mark IPv4 Link-Local

- (void)testIPv4MethodLinkLocal
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *servicePropertiesFromStore = nil;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodLinkLocal)
		}
	};
#define IPV4_LINKLOCAL_ADDR_PREFIX "169.254"
#define IPV4_LINKLOCAL_SUBNET_MASK "255.255.0.0"

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service), done, "Failed to create IPv4 service");
	[[self base] setService:service];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], SCD_KEY_IPCONFIGSERVICEKEY_CFARRAYREF(service), SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");
	servicePropertiesFromStore = (__bridge_transfer NSDictionary *)IPConfigurationServiceCopyInformation(service);

	// validates that the configuration we got is a valid link-local configuration
	// i.e. addr starts with 169.254 and subnet mask is 255.255.0.0
	XCT_REQUIRE_NONNULL(servicePropertiesFromStore, done, "Failed to retrieve active IPConfigurationService properties");
	XCT_REQUIRE_NONNULL(BRIDGED_ID(isA_CFDictionary((__bridge CFDictionaryRef)servicePropertiesFromStore)),
			    done, "Failed to get a CFDictionary type from the SCDynamicStore");
	NSLogDebug(@"New IPConfigurationService properties from the store:\n%@", servicePropertiesFromStore);
	XCTAssertEqualObjects(self.base.ifname,
			      [[servicePropertiesFromStore objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)] objectForKey:BRIDGED_NSSTRINGREF(kSCPropInterfaceName)],
			      "Failed to assert network interface name");
	XCTAssertTrue([[[[servicePropertiesFromStore
			  objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			 objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4Addresses)]
			objectAtIndex:0]
		       containsString:@IPV4_LINKLOCAL_ADDR_PREFIX],
		      "Failed to assert link-local address");
	XCTAssertEqualObjects(@[@IPV4_LINKLOCAL_SUBNET_MASK],
			      [[servicePropertiesFromStore
				objectForKey:BRIDGED_NSSTRINGREF(kSCEntNetIPv4)]
			       objectForKey:BRIDGED_NSSTRINGREF(kSCPropNetIPv4SubnetMasks)],
			      "Failed to assert link-local subnet mask");

done:
	[[self base] setService:NULL];
	RELEASE_NULLSAFE(service);
}

#pragma mark -
#pragma mark DHCPv4

- (void)testIPv4MethodDHCPWithClientID
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodDHCP),
#define DHCP_CLIENT_ID "0x0246414b4500"
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4DHCPClientID) : @DHCP_CLIENT_ID
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get feth interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNotNil(BRIDGED_ID(service), "Failed to create DHCPv4 service");
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

- (void)testIPv4MethodDHCP
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service = NULL;
	NSDictionary *customServiceProperties = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodDHCP)
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get feth interface");
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties);
	service = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties);
	XCTAssertNotNil(BRIDGED_ID(service), "Failed to create DHCPv4 service");
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

@end
