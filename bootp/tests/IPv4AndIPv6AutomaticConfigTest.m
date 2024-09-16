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

@interface IPv4AndIPv6AutomaticConfigTest : XCTestCase
@property (nonatomic) IPConfigurationFrameworkTestBase * base;
@end

@implementation IPv4AndIPv6AutomaticConfigTest

- (void)setUp
{
	[self setBase:[IPConfigurationFrameworkTestBase sharedInstance]];
}

- (void)tearDown
{
	// pass
}


#pragma mark -
#pragma mark DHCPv4 and AUTOMATIC-V6

- (void)testIPv4AndIPv6DHCPv4AndAutomaticv6
{
	long err = 0;
	bool setKeys = false;
	IPConfigurationServiceRef service4 = NULL;
	IPConfigurationServiceRef service6 = NULL;
	NSDictionary *customServiceProperties4 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv4Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv4ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv4ConfigMethodDHCP)
		}
	};
	NSDictionary *customServiceProperties6 = @{
		BRIDGED_NSSTRINGREF(kIPConfigurationServiceOptionIPv6Entity) : @{
			BRIDGED_NSSTRINGREF(kSCPropNetIPv6ConfigMethod) : BRIDGED_NSSTRINGREF(kSCValNetIPv6ConfigMethodAutomatic)
		}
	};

	XCT_REQUIRE_NONNULL(self.base.ifname, done, "Failed to get available interface");

	// sets up DHCPv4 service:
	// 	expects a change in 'State:/Network/Interface/tst$X/IPConfigurationBusy'
	// 	instead of in 'Plugin:IPConfigurationService:$UUID'
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties4);
	service4 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties4);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service4), done, "Failed to create DHCP service");
	[[self base] setService:service4];
	[[self base] setAlternativeValidation:TRUE];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service4));
	setKeys = SCDynamicStoreSetNotificationKeys([[self base] store], NULL, SCD_KEY_PATTERNS_CFARRAYREF);
	XCT_REQUIRE_TRUE(setKeys, done, "Failed to set SCDynamicStore notification keys");
	err = dispatch_semaphore_wait([[self base] serviceSem], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");

	// sets up AUTOMATIC-V6 on same interface:
	//	expects a change in 'State:/Network/Interface/tst$X/IPv6'
	// 	instead of in 'Plugin:IPConfigurationService:$UUID'
	NSLogDebug(@"Custom properties for new IPConfigurationService on '%@':\n%@", self.base.ifname, customServiceProperties6);
	service6 = IPConfigurationServiceCreate((__bridge CFStringRef)self.base.ifname, (__bridge CFDictionaryRef)customServiceProperties6);
	XCT_REQUIRE_NONNULL(BRIDGED_ID(service4), done, "Failed to create AUTOMATIC-V6 service");
	[[self base] setService2:service6];
	[[self base] setAlternativeValidation:TRUE];
	NSLogDebug(@"Successfully instantiated IPConfigurationService with key '%@'", IPConfigurationServiceGetNotificationKey(service6));
	err = dispatch_semaphore_wait([[self base] serviceSem2], dispatch_time(DISPATCH_TIME_NOW, SEMAPHORE_TIMEOUT_NANOSEC));
	XCT_REQUIRE_NERROR(err, done, "Semaphore timed out waiting for service to be up");


done:
	[[self base] setService:NULL];
	[[self base] setService2:NULL];
	RELEASE_NULLSAFE(service4);
	RELEASE_NULLSAFE(service6);
}

@end
