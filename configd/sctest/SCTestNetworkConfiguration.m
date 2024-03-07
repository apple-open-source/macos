/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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

#import "SCTest.h"
#import "SCTestUtils.h"

#define TEST_DOMAIN				"apple.biz"
#define kVPNProtocolPayloadInfo			CFSTR("com.apple.payload")
#define kSCTestNetworkProtocolTypeInvalid	CFSTR("Invalid")
#define kSCTestNetworkServiceName		CFSTR("ServiceTest")
#define kSCTestNetworkSetName			CFSTR("SetTest")
#if TARGET_OS_OSX
#define kSCTestNetworkBondInterfaceName		CFSTR("BondTest")
#define kSCTestNetworkVlanInterfaceName		CFSTR("VLANTest")
#define kSCTestNetworkVBridgeInterfaceName	CFSTR("BridgeTest")
#endif

@interface SCTestNetworkConfiguration : SCTest
@property SCPreferencesRef prefs;
@end

#if !TARGET_OS_BRIDGE
@implementation SCTestNetworkConfiguration

+ (NSString *)command
{
	return @"network configuration";
}

+ (NSString *)commandDescription
{
	return @"Tests the SCNetworkConfiguration code path";
}

- (instancetype)initWithOptions:(NSDictionary *)options
{
	self = [super initWithOptions:options];
	if (self) {
		self.prefs = SCPreferencesCreateWithOptions(kCFAllocatorDefault,
							CFSTR("SCTest"),
							CFSTR("SCTestPreferences.plist"),
							kSCPreferencesUseEntitlementAuthorization,
							(__bridge CFDictionaryRef)@{(__bridge NSString *)kSCPreferencesOptionRemoveWhenEmpty:(__bridge NSNumber *)kCFBooleanTrue});
		if (self.prefs != NULL) {
			[self populateDefaultPrefs];

			if (!SCPreferencesCommitChanges(self.prefs)) {
				SCTestLog("Failed to commit default prefs");
				ERR_EXIT;
			}
		} else {
			SCTestLog("No prefs session to test network configuration APIs");
			ERR_EXIT;
		}
	}
	return self;
}

- (void)dealloc
{
	if (self.prefs != NULL) {
		SCPreferencesSynchronize(self.prefs);
		[self removeAllValuesAndCommitPreferences];
		CFRelease(self.prefs);
		self.prefs = NULL;
	}
}

- (void)start
{
	[self cleanupAndExitWithErrorCode:0];
}

- (void)cleanupAndExitWithErrorCode:(int)error
{
	[super cleanupAndExitWithErrorCode:error];
}

- (BOOL)commitAndUnlockPreferences
{
	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences");
		return NO;
	}
	if (!SCPreferencesUnlock(self.prefs)) {
		SCTestLog("Failed to unlock preferences");
		return NO;
	}
	return YES;
}

- (void)populateDefaultPrefs
{
	SCNetworkSetRef	currentSet;

	currentSet = SCNetworkSetCopyCurrent(self.prefs);
	if (currentSet == NULL) {
		currentSet = _SCNetworkSetCreateDefault(self.prefs);
	}
	SCNetworkSetEstablishDefaultConfiguration(currentSet);
	CFRelease(currentSet);

	return;
}

static void
my_CFRelease(void *t)
{
	void * * obj = (void * *)t;
	if (obj && *obj) {
		CFRelease(*obj);
		*obj = NULL;
	}
	return;
}

- (BOOL)removeAllValuesAndCommitPreferences
{
	BOOL ok = NO;

	if (self.prefs == NULL) {
		SCTestLog("Error: No preferences session.");
		return ok;
	}
	ok = SCPreferencesRemoveAllValues(self.prefs);
	if (!ok) {
		SCTestLog("Failed to remove all values from preferences. Error: %s", SCErrorString(SCError()));
		return ok;
	}
	ok = SCPreferencesCommitChanges(self.prefs);
	if (!ok) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		return ok;
	}
	return ok;
}

- (SCNetworkInterfaceRef)copyInterface:(NSArray *)services
{
	SCNetworkServiceRef service = NULL;
	SCNetworkInterfaceRef interface = NULL;
	SCNetworkInterfaceRef wifiInterface = NULL;
	SCNetworkInterfaceRef otherInterface = NULL;
	CFStringRef interfaceType = NULL;

	if (services == nil) {
		return NULL;
	}

	for (id servicePtr in services) {
		service = (__bridge SCNetworkServiceRef)servicePtr;
		interface = SCNetworkServiceGetInterface(service);
		if (interface == nil) {
			continue;
		}
		interfaceType = SCNetworkInterfaceGetInterfaceType(interface);
		if (CFEqual(interfaceType, kSCNetworkInterfaceTypeIEEE80211)) {
			wifiInterface = interface;
			break;
		} else {
			otherInterface = interface;
		}
	}

	if (wifiInterface != NULL) {
		return CFRetain(wifiInterface);
	} else if (otherInterface != NULL) {
		return CFRetain(otherInterface);
	}

	return otherInterface;
}

- (BOOL)unitTest
{
	BOOL allUnitTestsPassed = YES;
	allUnitTestsPassed &= [self unitTestNetworkInterfaceConfiguration];
#if TARGET_OS_OSX
	allUnitTestsPassed &= [self unitTestNetworkVLANInterfaceConfiguration];
	allUnitTestsPassed &= [self unitTestNetworkEthernetBondInterfaceConfiguration];
	allUnitTestsPassed &= [self unitTestNetworkBridgeInterfaceConfiguration];
#endif
	allUnitTestsPassed &= [self unitTestNetworkServiceConfiguration];
	allUnitTestsPassed &= [self unitTestNetworkSetConfiguration];
	allUnitTestsPassed &= [self unitTestNetworkProtocolConfiguration];
	return  allUnitTestsPassed;
}

#pragma mark -
#pragma mark Interface Configuration APIs

- (BOOL)unitTestNetworkInterfaceConfiguration
{
	NSString *subtype = nil;
	NSString *subtypeOption = nil;
	NSArray *services = nil;
	NSMutableArray *configOptions = nil;
	NSMutableDictionary *newIfConfig = nil;
	NSMutableDictionary *newIfExtendedConfig = nil;
	CFArrayRef available = NULL;
	CFDictionaryRef extendedConfiguration = NULL;
	CFDictionaryRef ifConfig = NULL;
	SCNetworkServiceRef service = NULL;
	SCNetworkInterfaceRef interface = NULL;
	NSUUID *uuid = [NSUUID UUID];
	BOOL prefsLocked = NO;
	BOOL status = NO;
	BOOL subtypeOptionsSet = NO;
	int currentMTU;
	int minMTU;
	int maxMTU;

	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	if (services != nil) {
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			interface = SCNetworkServiceGetInterface(service);
			if (interface != nil && (CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypeEthernet))) {
				break;
			}
		}
		if (interface == NULL) {
			SCTestLog("No Ethernet interface to test interface configuration");
			goto done;
		}
	} else 	{
		SCTestLog("No services in prefs.");
		goto done;
	}

	if (!SCPreferencesLock(self.prefs, true)) {
		return NO;
	}
	prefsLocked = YES;

	/* NETWORK INTERFACE CONFIGURATION APIs */
	newIfConfig = [[NSMutableDictionary alloc] init];
	[newIfConfig setObject:(__bridge NSString *)kSCValNetInterfaceTypeEthernet forKey:(__bridge NSString *)kSCPropNetInterfaceHardware];
	if (!SCNetworkInterfaceSetConfiguration(interface, (__bridge CFDictionaryRef)newIfConfig)) {
		SCTestLog("SCNetworkInterfaceSetConfiguration() failed to set interface configuration. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	newIfExtendedConfig = [[NSMutableDictionary alloc] init];
	[newIfExtendedConfig setObject:@"Test" forKey:@"PayloadOrganization"];
	[newIfExtendedConfig setObject:uuid.UUIDString forKey:@"PayloadUUID"];
	if (!SCNetworkInterfaceSetExtendedConfiguration(interface, kVPNProtocolPayloadInfo, (__bridge CFDictionaryRef)newIfExtendedConfig)) {
		SCTestLog("SCNetworkInterfaceSetExtendedConfiguration() failed to set extended configuration. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkInterfaceSetMTU(interface, 0)) {
		SCTestLog("SCNetworkInterfaceSetMTU() failed to set MTU. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	subtype = @"100baseTX";
	subtypeOption = @"full-duplex";
	configOptions = [[NSMutableArray alloc] init];
	[configOptions addObject:subtypeOption];
	if (!SCNetworkInterfaceCopyMediaOptions(interface, NULL, NULL, &available, FALSE)) {
		SCTestLog("SCNetworkInterfaceCopyMediaOptions() failed to get available media options for interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	if (available != NULL) {
		NSArray *allSubtypes;
		allSubtypes = (__bridge_transfer NSArray *)SCNetworkInterfaceCopyMediaSubTypes(available);
		if ((allSubtypes != nil) && [allSubtypes containsObject:subtype]) {
			NSArray *subtype_options = nil;
			subtype_options = (__bridge_transfer NSArray *)SCNetworkInterfaceCopyMediaSubTypeOptions(available, (__bridge CFStringRef)subtype);
			if ((subtype_options != nil) && [subtype_options containsObject:configOptions]) {
				if (!SCNetworkInterfaceSetMediaOptions(interface, (__bridge CFStringRef)subtype, (__bridge CFArrayRef)configOptions)) {
					SCTestLog("SCNetworkInterfaceSetMediaOptions() failed to set media options for interface. Error: %s", SCErrorString(SCError()));
					goto done;
				}
				subtypeOptionsSet = YES;
			}
		}
		my_CFRelease(&available);
	}

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after interface configuration. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Interface Configuration Set APIs */

	prefsLocked = NO;

	/* VALIDATE INTERFACE CONFIGURATION APIs */
	ifConfig = SCNetworkInterfaceGetConfiguration(interface);
	if (ifConfig == NULL) {
		SCTestLog("SCNetworkInterfaceSetConfiguration() validation: SCNetworkInterfaceGetConfiguration() failed to get interface configuration. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	extendedConfiguration = SCNetworkInterfaceGetExtendedConfiguration(interface, kVPNProtocolPayloadInfo);
	if (extendedConfiguration == NULL || !CFEqual(extendedConfiguration, (__bridge CFDictionaryRef)newIfExtendedConfig)) {
		SCTestLog("SCNetworkInterfaceSetExtendedConfiguration() validation: SCNetworkInterfaceGetExtendedConfiguration() failed to get extended configuration.");
		goto done;
	}

	if (!SCNetworkInterfaceCopyMTU(interface, &currentMTU, &minMTU, &maxMTU)) {
		SCTestLog("SCNetworkInterfaceSetMTU() validation:  SCNetworkInterfaceCopyMTU() failed to get MTU for interface.");
		goto done;
	}

	if (subtypeOptionsSet) {
		if (!SCNetworkInterfaceCopyMediaOptions(interface, NULL, NULL, &available, FALSE)) {
			SCTestLog("SCNetworkInterfaceSetMediaOptions() validation: SCNetworkInterfaceCopyMediaOptions() failed to get available media options for interface.");
			goto done;
		}
		if (available != NULL) {
			NSArray *subtype_options;
			subtype_options = (__bridge_transfer NSArray *)SCNetworkInterfaceCopyMediaSubTypeOptions(available, (__bridge CFStringRef)subtype);
			if ((subtype_options != nil) && ![subtype_options containsObject:configOptions]) {
				SCTestLog("SCNetworkInterfaceSetMediaOptions() validation: SCNetworkInterfaceCopyMediaSubTypeOptions() returned incorrect options for a given subtype.");
				goto done;
			}
			my_CFRelease(&available);
		}
	}

	if (!isA_CFType(interface, SCNetworkInterfaceGetTypeID())){
		SCTestLog("SCNetworkInterfaceGetTypeID() failed to get a interface Type ID");
		goto done;
	}
	
	SCTestLog("Verified that SCNetworkInterface APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&available);
	return status;
}

#if TARGET_OS_OSX
- (BOOL)unitTestNetworkVLANInterfaceConfiguration
{
	NSArray *vlanInterfacesCopy = nil;
	NSArray *availablePhysicalInterfaces = nil;
	NSDictionary *getOptions = nil;
	CFMutableDictionaryRef vlanOptions = NULL;
	CFNumberRef vlanID = NULL;
	CFNumberRef validateVlanTag = NULL;
	SCVLANInterfaceRef vlanInterface = NULL;
	SCNetworkInterfaceRef interface = NULL;
	SCNetworkInterfaceRef validateInterface = NULL;
	int vlan;
	BOOL status = NO;
	BOOL prefsLocked = NO;

	availablePhysicalInterfaces = (__bridge_transfer NSArray *)SCVLANInterfaceCopyAvailablePhysicalInterfaces();
	if (availablePhysicalInterfaces != nil) {
		for (id member in availablePhysicalInterfaces) {
			interface = (__bridge SCNetworkInterfaceRef)member;
		}
	}
	if (interface == NULL) {
		SCTestLog("No physical interface to create a VLAN interface.");
		status = YES;
		goto done;
	}

	if (!SCPreferencesLock(self.prefs, true)) {
		return NO;
	}
	prefsLocked = YES;

	/* NETWORK VLAN INTERFACE APIs */
	vlan = 500;
	vlanID = CFNumberCreate(NULL, kCFNumberIntType, &vlan);
	vlanInterface = SCVLANInterfaceCreate(self.prefs, interface, vlanID);
	if (vlanInterface == NULL) {
		SCTestLog("SCVLANInterfaceCreate() failed to create a VLAN interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCVLANInterfaceSetLocalizedDisplayName(vlanInterface, kSCTestNetworkVlanInterfaceName)) {
		SCTestLog("SCVLANInterfaceSetLocalizedDisplayName() failed to set name. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	vlanOptions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(vlanOptions, CFSTR("Name"), CFSTR("VLANTest"));
	if (!SCVLANInterfaceSetOptions(vlanInterface, vlanOptions)) {
		SCTestLog("SCVLANInterfaceSetOptions() failed to set config method. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCVLANInterfaceSetPhysicalInterfaceAndTag(vlanInterface, interface, vlanID)) {
		SCTestLog("SCVLANInterfaceSetPhysicalInterfaceAndTag() failed to set physical interface and tag. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after adding a new VLAN interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of VLAN interface set APIs */

	prefsLocked = NO;

	/* VALIDATE VLAN INTERFACE APIs */
	vlanInterfacesCopy = (__bridge_transfer NSArray *)SCVLANInterfaceCopyAll(self.prefs);
	if (vlanInterfacesCopy == nil || !CFArrayContainsValue((__bridge CFArrayRef)vlanInterfacesCopy,
							       CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)vlanInterfacesCopy)),
							       vlanInterface)) {
		SCTestLog("SCVLANInterfaceCreate() validation: SCVLANInterfaceCopyAll() failed to retrieve a new VLAN interface from prefs.");
		goto done;
	}

	getOptions = (__bridge NSDictionary *)SCVLANInterfaceGetOptions(vlanInterface);
	if (getOptions == nil || !CFEqual((__bridge CFDictionaryRef)getOptions, vlanOptions)) {
		SCTestLog("SCVLANInterfaceSetOptions() validation: SCVLANInterfaceGetOptions() failed to get the right options for vlan interface.");
		goto done;
	}

	validateInterface = SCVLANInterfaceGetPhysicalInterface(vlanInterface);
	if (validateInterface == NULL) {
		SCTestLog("SCVLANInterfaceGetPhysicalInterface failed to get physical interface.");
		goto done;
	}
	if (!_SC_CFEqual(interface, validateInterface)) {
		SCTestLog("SCVLANInterfaceSetPhysicalInterfaceAndTag() validation: failed to get physical interface from VLAN interface.");
		goto done;
	}

	validateVlanTag = SCVLANInterfaceGetTag(vlanInterface);
	if (validateVlanTag == NULL) {
		SCTestLog("SCVLANInterfaceSetPhysicalInterfaceAndTag() validation: SCVLANInterfaceGetTag() failed to return VLAN tag.");
		goto done;
	}
	if (!CFEqual(vlanID, validateVlanTag)) {
		SCTestLog("SCVLANInterfaceSetPhysicalInterfaceAndTag() validation: SCVLANInterfaceGetTag() failed to get the right VLAN tag from VLAN interface.");
		goto done;
	}

	/* REMOVE VLAN INTERFACE */
	if (!SCVLANInterfaceRemove(vlanInterface)) {
		SCTestLog("SCVLANInterfaceRemove() failed to remove a vlan interface. Error: %s", SCErrorString(SCError()));
	}

	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	/* Validate VLAN interface removal from prefs */
	vlanInterfacesCopy = (__bridge_transfer NSArray *)SCVLANInterfaceCopyAll(self.prefs);
	if (vlanInterfacesCopy != nil && CFArrayContainsValue((__bridge CFArrayRef)vlanInterfacesCopy,
							      CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)vlanInterfacesCopy)),
							      vlanInterface)) {
		SCTestLog("SCVLANInterfaceRemove() validation: SCVLANInterfaceCopyAll() retrieved a deleted VLAN interface from prefs.");
		goto done;
	}

	SCTestLog("Verified that SCNetworkVLANInterface APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&vlanInterface);
	my_CFRelease(&vlanID);
	my_CFRelease(&vlanOptions);
	return status;
}

- (BOOL)unitTestNetworkEthernetBondInterfaceConfiguration
{
	NSArray *services = NULL;
	NSArray *interfaces = NULL;
	NSArray *bondInterfacesCopy = nil;
	NSArray *availableMemberInterfaces = nil;
	CFMutableArrayRef memberInterfaces = NULL;
	CFMutableDictionaryRef bondOptions = NULL;
	CFDictionaryRef getOptions = nil;
	CFNumberRef mode = NULL;
	SCBondInterfaceRef bondInterface = NULL;
	SCNetworkInterfaceRef interface = NULL;
	const int bondMode = 1;
	int interfaceCount = 0;
	BOOL status = NO;
	BOOL prefsLocked = NO;

	availableMemberInterfaces = (__bridge_transfer NSArray *)SCBondInterfaceCopyAvailableMemberInterfaces(self.prefs);
	if (availableMemberInterfaces != nil) {
		SCNetworkInterfaceRef interface = NULL;
		memberInterfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (id member in availableMemberInterfaces) {
			interface = (__bridge SCNetworkInterfaceRef)member;
			CFArrayAppendValue(memberInterfaces, interface);
			interfaceCount++;
			//Add two member interfaces
			if (interfaceCount == 2) break;
		}
	}

	if ((memberInterfaces == NULL) || (interfaceCount != 2))
	{
		SCTestLog("No member interfaces to test Ethernet Bond interface.");
		status = YES;
		goto done;
	}

	/* Interface with a service cannot be used as a member for bond interface */
	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	if (services != nil) {
		SCNetworkInterfaceRef interface;
		SCNetworkServiceRef service;
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			interface = SCNetworkServiceGetInterface(service);
			if (CFArrayContainsValue(memberInterfaces, CFRangeMake(0, CFArrayGetCount(memberInterfaces)), interface)) {
				SCNetworkServiceRemove(service);
			}
		}
		if (!SCPreferencesCommitChanges(self.prefs)) {
			SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
			goto done;
		}
	}

	if (!SCPreferencesLock(self.prefs, true)) {
		return NO;
	}
	prefsLocked = YES;

	/* ETHERNET BOND INTERFACE SET APIs */
	bondInterface = SCBondInterfaceCreate(self.prefs);
	if (bondInterface == NULL) {
		SCTestLog("SCBondInterfaceCreate() failed to create a bond interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCBondInterfaceSetLocalizedDisplayName(bondInterface, kSCTestNetworkBondInterfaceName)) {
		SCTestLog("SCBondInterfaceSetLocalizedDisplayName() failed to set name. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (memberInterfaces != NULL)
	{
		if (!SCBondInterfaceSetMemberInterfaces(bondInterface, memberInterfaces)) {
			SCTestLog("SCBondInterfaceSetMemberInterfaces() failed to set member interfaces. Error: %s", SCErrorString(SCError()));
			goto done;
		}
	}

	bondOptions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(bondOptions, CFSTR("__AUTO__"), CFSTR("Ethernet"));
	if (!SCBondInterfaceSetOptions(bondInterface, bondOptions)) {
		SCTestLog("SCBondInterfaceSetBondOptions() failed to set config method. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	mode = CFNumberCreate(NULL,kCFNumberIntType, &bondMode);
	if (!SCBondInterfaceSetMode(bondInterface, mode)) {
		SCTestLog("SCBondInterfaceSetMode() failed to set mode for bond interface. Error: %s", SCErrorString(SCError()));
		my_CFRelease(&mode);
		goto done;
	}
	my_CFRelease(&mode);

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after adding a new bond interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Ethernet Bond interface set APIs */

	prefsLocked = NO;

	/* VALIDATE ETHERNET BOND INTERFACE APIs */
	bondInterfacesCopy = (__bridge_transfer NSArray *)SCBondInterfaceCopyAll(self.prefs);
	if (bondInterfacesCopy == nil || !CFArrayContainsValue((__bridge CFArrayRef)bondInterfacesCopy,
							       CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)bondInterfacesCopy)),
							       bondInterface)) {
		SCTestLog("Bond interfaces copy %@", bondInterfacesCopy);
		SCTestLog("SCBondInterfaceCreate() validation: SCBondInterfaceCopyAll() failed to retrieve a new bond interface from prefs.");
		goto done;
	}

	mode = SCBondInterfaceGetMode(bondInterface);
	if (mode == NULL) {
		SCTestLog("SCBondInterfaceSetMode() validation:  SCBondInterfaceGetMode() returned incorrect mode.");
		goto done;
	}

	getOptions = SCBondInterfaceGetOptions(bondInterface);
	if (getOptions == nil || !CFEqual(getOptions, bondOptions)) {
		SCTestLog("SCBondInterfaceSetOptions() validation:  SCBondInterfaceGetOptions() failed to get the right options for bond interface.");
		goto done;
	}

	interfaces = (__bridge NSArray *)SCBondInterfaceGetMemberInterfaces(bondInterface);
	if (interfaces == nil) {
		SCTestLog("SCBondInterfaceSetMemberInterfaces() validation: SCBondInterfaceGetMemberInterfaces() failed to return member interfaces from bond interface.");
		goto done;
	}
	for (CFIndex i=0; i<CFArrayGetCount(memberInterfaces); i++) {
		interface = CFArrayGetValueAtIndex(memberInterfaces, i);
		if (!CFArrayContainsValue((__bridge CFArrayRef)interfaces,
					  CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)interfaces)),
					  interface)) {
			SCTestLog("SCBondInterfaceSetMemberInterfaces() validation: SCBondInterfaceGetMemberInterfaces() failed to get the right member interfaces.");
			goto done;
		}
	}

	/* REMOVE BOND INTERFACE */
	if (!SCBondInterfaceRemove(bondInterface)) {
		SCTestLog("SCBondInterfaceRemove() failed to remove a bond interface. Error: %s", SCErrorString(SCError()));
	}

	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	bondInterfacesCopy = (__bridge_transfer NSArray *)SCBondInterfaceCopyAll(self.prefs);
	if (bondInterfacesCopy != nil && CFArrayContainsValue((__bridge CFArrayRef)bondInterfacesCopy,
							      CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)bondInterfacesCopy)),
							      bondInterface)) {
		CFStringRef bsdName = SCNetworkInterfaceGetBSDName(bondInterface);
		SCTestLog("Bondinterfaces copy %@", bondInterfacesCopy);
		SCTestLog("SCBondInterfaceRemove() validation: SCBondInterfaceCopyAll() retrieved a deleted bond interface %@ from prefs.", bsdName);
		goto done;
	}

	SCTestLog("Verified that SCNetworkEthernetBondInterface APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&bondInterface);
	my_CFRelease(&bondOptions);
	my_CFRelease(&memberInterfaces);
	return status;
}

- (BOOL)unitTestNetworkBridgeInterfaceConfiguration
{
	NSArray *bridgeInterfacesCopy = nil;
	NSArray *availableMemberInterfaces = nil;
	NSArray *services = nil;
	NSArray *interfaces = nil;
	NSDictionary *getOptions = nil;
	CFMutableDictionaryRef bridgeOptions = NULL;
	CFMutableArrayRef memberInterfaces = NULL;
	SCBridgeInterfaceRef bridgeInterface = NULL;
	SCNetworkInterfaceRef interface = NULL;
	int interfaceCount = 0;
	BOOL status = NO;
	BOOL prefsLocked = NO;

	availableMemberInterfaces = (__bridge_transfer NSArray *)SCBridgeInterfaceCopyAvailableMemberInterfaces(self.prefs);
	if (availableMemberInterfaces != nil) {
		SCNetworkInterfaceRef interface = NULL;
		memberInterfaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (id member in availableMemberInterfaces) {
			interface = (__bridge SCNetworkInterfaceRef)member;
			CFArrayAppendValue(memberInterfaces, interface);
			interfaceCount++;
			//Add two member interfaces
			if (interfaceCount == 2) break;
		}
	}

	if ((memberInterfaces == NULL) || (interfaceCount != 2))
	{
		SCTestLog("No member interfaces to test Bridge interface.");
		status = YES;
		goto done;
	}

	/* Interface with a service cannot be used as a member for bridge interface */
	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	if (services != nil) {
		SCNetworkInterfaceRef interface;
		SCNetworkServiceRef service;
		for (id servicePtr in services) {
			service = (__bridge SCNetworkServiceRef)servicePtr;
			interface = SCNetworkServiceGetInterface(service);
			if (CFArrayContainsValue(memberInterfaces, CFRangeMake(0, CFArrayGetCount(memberInterfaces)), interface)) {
				SCNetworkServiceRemove(service);
			}
		}
		if (!SCPreferencesCommitChanges(self.prefs)) {
			SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
			goto done;
		}
	}

	if (!SCPreferencesLock(self.prefs, true)) {
		return NO;
	}
	prefsLocked = YES;

	/* BRIDGE INTERFACE APIs */
	bridgeInterface = SCBridgeInterfaceCreate(self.prefs);
	if (bridgeInterface == NULL) {
		SCTestLog("SCBridgeInterfaceCreate() failed to create a bridge interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCBridgeInterfaceSetLocalizedDisplayName(bridgeInterface, kSCTestNetworkVlanInterfaceName)) {
		SCTestLog("SCBridgeInterfaceSetLocalizedDisplayName() failed to set name. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCBridgeInterfaceSetMemberInterfaces(bridgeInterface, memberInterfaces)) {
		SCTestLog("SCBridgeInterfaceSetMemberInterfaces() failed to set member interfaces for a bridge interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	bridgeOptions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(bridgeOptions, CFSTR("__AUTO__"), CFSTR("Thunderbolt-bridge"));
	if (!SCBridgeInterfaceSetOptions(bridgeInterface, bridgeOptions)) {
		SCTestLog("SCBridgeInterfaceSetOptions() failed to set bridge options. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCBridgeInterfaceSetAllowConfiguredMembers(bridgeInterface, TRUE)){
		SCTestLog("SCBridgeInterfaceSetAllowConfiguredMembers() failed to allow configured members for a bridge interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	CFDictionarySetValue(bridgeOptions, CFSTR("AllowConfiguredMembers"), kCFBooleanTrue); //For validation

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after adding a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Bridge interface set APIs */

	prefsLocked = NO;

	/* VALIDATE BRIDGE INTERFACE APIs */
	bridgeInterfacesCopy = (__bridge_transfer NSArray *)SCBridgeInterfaceCopyAll(self.prefs);
	if (bridgeInterfacesCopy == nil || !CFArrayContainsValue((__bridge CFArrayRef)bridgeInterfacesCopy,
							       CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)bridgeInterfacesCopy)),
							       bridgeInterface)) {
		SCTestLog("SCBridgeInterfaceCreate() validation: SCBridgeInterfaceCopyAll() failed to retrieve a new bridge interface from prefs.");
		goto done;
	}

	getOptions = (__bridge NSDictionary *)SCBridgeInterfaceGetOptions(bridgeInterface);
	if (getOptions == NULL || !CFEqual((__bridge CFDictionaryRef)getOptions, bridgeOptions)) {
		SCTestLog("SCBridgeInterfaceSetOptions() validation: SCBridgeInterfaceGetOptions() failed to get the right options for bridge interface.");
		goto done;
	}

	interfaces = (__bridge NSArray *)SCBridgeInterfaceGetMemberInterfaces(bridgeInterface);
	if (interfaces == nil) {
		SCTestLog("SCBridgeInterfaceSetMemberInterfaces() validation: SCBridgeInterfaceGetMemberInterfaces() failed to return member interfaces from a bridge interface.");
		goto done;
	}
	for (CFIndex i=0; i<CFArrayGetCount(memberInterfaces); i++) {
		interface = CFArrayGetValueAtIndex(memberInterfaces, i);
		if (!CFArrayContainsValue((__bridge CFArrayRef)interfaces,
					  CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)interfaces)),
					  interface)) {
			SCTestLog("SCBridgeInterfaceSetMemberInterfaces() validation: SCBridgeGetMemberInterfaces() returned incorrect set of member interfaces.");
			goto done;
		}
	}
	my_CFRelease(&memberInterfaces);

	/* REMOVE BRIDGE INTERFACE */
	if (!SCBridgeInterfaceRemove(bridgeInterface)) {
		SCTestLog("SCBridgeInterfaceRemove() failed to remove a bridge interface.");
		goto done;
	}

	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	SCPreferencesSynchronize(self.prefs);

	/* Validate bridge interface removal from prefs */
	bridgeInterfacesCopy = (__bridge_transfer NSArray *)SCVLANInterfaceCopyAll(self.prefs);
	if (bridgeInterfacesCopy != nil && CFArrayContainsValue((__bridge CFArrayRef)bridgeInterfacesCopy,
								CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)bridgeInterfacesCopy)),
								bridgeInterface)) {
		SCTestLog("SCBridgeInterfaceRemove() validation: SCBridgeInterfaceCopyAll() retrieved a deleted bridge interface from prefs.");
		goto done;
	}

	SCTestLog("Verified that SCNetworkBridgeInterface APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&bridgeInterface);
	my_CFRelease(&bridgeOptions);
	my_CFRelease(&memberInterfaces);
	return status;
}
#endif // TARGET_OS_OSX

#pragma mark -
#pragma mark Network Service And Set APIs

- (BOOL)unitTestNetworkServiceConfiguration
{
	NSString *serviceName = nil;
	NSArray *services = nil;
	NSArray *protocols = nil;
	CFStringRef serviceID = NULL;
	SCNetworkInterfaceRef interface = NULL;
	SCNetworkInterfaceRef serviceInterface = NULL;
	SCNetworkServiceRef newService = NULL;
	SCNetworkProtocolRef ipv4Protocol = NULL;
	BOOL prefsLocked = NO;
	BOOL status = NO;

	if (!SCPreferencesLock(self.prefs, true)) {
		SCTestLog("no lock");
		return NO;
	}
	prefsLocked = YES;

	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	interface = [self copyInterface:services];
	if (interface == NULL) {
		SCTestLog("Failed to find an interface in prefs.");
		goto done;
	}

	/* NETWORK SERVICE APIs */
	newService = SCNetworkServiceCreate(self.prefs, interface);
	if (newService == NULL) {
		SCTestLog("SCNetworkServiceCreate() failed to create a new service for a given interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkServiceEstablishDefaultConfiguration(newService)) {
		SCTestLog("SCNetworkServiceEstablishDefaultConfiguration() failed to establish default configuration for a new service. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkServiceSetName(newService, kSCTestNetworkServiceName)) {
		SCTestLog("SCNetworkServiceSetName() failed to set a service name. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkServiceSetEnabled(newService, TRUE)) {
		SCTestLog("SCNetworkServiceSetEnabled() failed to enable a service. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkServiceAddProtocolType(newService, kSCNetworkProtocolTypeIPv4)) {
		if (!SCNetworkServiceRemoveProtocolType(newService, kSCNetworkProtocolTypeIPv4)) {
			SCTestLog("SCNetworkServiceRemoveProtocolType() failed to remove a valid protocol type from a service. Error: %s", SCErrorString(SCError()));
			goto done;
		}
		if (!SCNetworkServiceAddProtocolType(newService, kSCNetworkProtocolTypeIPv4)) {
			SCTestLog("SCNetworkServiceAddProtocolType() failed to add a valid protocol type to a service. Error: %s", SCErrorString(SCError()));
			goto done;
		}
	}

	/* Add invalid protocol type */
	if (SCNetworkServiceAddProtocolType(newService, kSCTestNetworkProtocolTypeInvalid)) {
		SCTestLog("SCNetworkServiceAddProtocolType() added invalid protocol type to a service.");
		goto done;
	}

	/* Retain Service ID for validation */
	serviceID = SCNetworkServiceGetServiceID(newService);
	if (serviceID == NULL) {
		SCTestLog("SCNetworkServiceGetServiceID() failed to get service ID for a new service. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	CFRetain(serviceID);

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after adding a new service. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Network Service Set APIs */

	prefsLocked = NO;
	my_CFRelease(&newService);

	/* VALIDATE NETWORK SERVICE APIs */
	SCPreferencesSynchronize(self.prefs);
	newService = SCNetworkServiceCopy(self.prefs, serviceID);
	if (newService == NULL) {
		SCTestLog("SCNetworkServiceCreate() validation: SCNetworkServiceCopy() failed to copy a new service.");
		goto done;
	}
	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	if (services == nil || !CFArrayContainsValue((__bridge CFArrayRef)services,
						      CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)services)),
						      newService)) {
		SCTestLog("SCNetworkServiceCreate() validation: SCNetworkServiceCopyAll() failed to retrieve a new service from prefs.");
		goto done;
	}

	serviceInterface = SCNetworkServiceGetInterface(newService);
	if (serviceInterface == NULL || !_SC_CFEqual(interface, serviceInterface)) {
		SCTestLog("SCNetworkServiceGetInterface() failed to get interface for a new service.");
		goto done;
	}

	serviceName = (__bridge NSString *)SCNetworkServiceGetName(newService);
	if ((serviceName == nil) || !CFEqual((__bridge CFStringRef)serviceName, kSCTestNetworkServiceName)) {
		SCTestLog("SCNetworkServiceSetName() validation: SCNetworkServiceGetName() failed to get a service name.");
		goto done;
	}

	if (!SCNetworkServiceGetEnabled(newService)) {
		SCTestLog("SCNetworkServiceSetEnabled() validation: SCNetworkServiceGetEnabled() returned FALSE for an enabled service.");
		goto done;
	}

	protocols = (__bridge_transfer NSArray *)SCNetworkServiceCopyProtocols(newService);
	if ([protocols count] == 0) {
		SCTestLog("SCNetworkServiceAddProtocolType() validation: SCNetworkServiceCopyProtocols() failed to copy protocols from a service.");
		goto done;
	}
	ipv4Protocol = SCNetworkServiceCopyProtocol(newService, kSCNetworkProtocolTypeIPv4);
	if (ipv4Protocol == NULL || !CFArrayContainsValue((__bridge CFArrayRef)protocols,
							  CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)protocols)),
							  ipv4Protocol)) {
		SCTestLog("SCNetworkServiceAddProtocolType() validation: SCNetworkServiceCopyProtocol() failed to copy protocol from a service.");
		goto done;
	}
	my_CFRelease(&ipv4Protocol);

	if (!isA_CFType(newService, SCNetworkServiceGetTypeID())) {
		SCTestLog("SCNetworkServiceGetTypeID() failed to get a service Type ID");
		goto done;
	}

	/* REMOVE SERVICE */
	if (!SCNetworkServiceRemove(newService)) {
		SCTestLog("SCNetworkServiceRemove() failed to remove a service. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	my_CFRelease(&newService);

	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	/* Validate service removal from prefs */
	newService = SCNetworkServiceCopy(self.prefs, serviceID);
	if (newService != NULL) {
		SCTestLog("SCNetworkServiceRemove() validation: SCNetworkServiceCopy() returned a removed service.");
		goto done;
	}
	SCTestLog("Verified that SCNetworkService APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&ipv4Protocol);
	my_CFRelease(&serviceID);
	my_CFRelease(&newService);
	my_CFRelease(&interface);
	return status;
}

- (BOOL)unitTestNetworkSetConfiguration
{
	NSString *setName = nil;
	NSString *serviceID = nil;
	NSString *setID = nil;
	NSArray *services = nil;
	NSArray *allSets = nil;
	CFStringRef newSetID = NULL;
	CFStringRef newServiceID = NULL;
	CFArrayRef currentServiceOrder = NULL;
	CFMutableArrayRef newServiceOrder = NULL;
	CFIndex currentServiceOrderCount = 0;
	SCNetworkSetRef oldSet = NULL;
	SCNetworkSetRef newSet = NULL;
	SCNetworkInterfaceRef interface = NULL;
	SCNetworkServiceRef newService = NULL;
	BOOL prefsLocked = NO;
	BOOL status = NO;

	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	interface = [self copyInterface:services];
	if (interface == NULL) {
		SCTestLog("Failed to find an interface in prefs.");
		goto done;
	}
	oldSet = SCNetworkSetCopyCurrent(self.prefs);
	if (oldSet != NULL) {
		currentServiceOrder = SCNetworkSetGetServiceOrder(oldSet);
		if (currentServiceOrder == NULL) {
			newServiceOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		} else {
			newServiceOrder = CFArrayCreateMutableCopy(NULL, 0, currentServiceOrder);
		}
	} else {
		SCTestLog("Failed to get current set from prefs.");
		goto done;
	}

	if (!SCPreferencesLock(self.prefs, true)) {
		SCTestLog("Failed to acquire prefs lock.");
		goto done;
	}
	prefsLocked = YES;

	/* NETWORK SET APIs */
	newSet = SCNetworkSetCreate(self.prefs);
	if (newSet == NULL) {
		SCTestLog("SCNetworkSetCreate() failed to create a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkSetSetName(newSet, kSCTestNetworkSetName)) {
		SCTestLog("SCNetworkSetSetName() failed to set name. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	newService = SCNetworkServiceCreate(self.prefs, interface);
	if (newService == NULL) {
		SCTestLog("SCNetworkServiceCreate() failed to create a new service for a given interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkServiceEstablishDefaultConfiguration(newService)) {
		SCTestLog("SCNetworkServiceEstablishDefaultConfiguration() failed to establish default configuration for a new service. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkSetAddService(newSet, newService)) {
		SCTestLog("SCNetworkSetAddService() failed to add a service to a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	newServiceID = SCNetworkServiceGetServiceID(newService);
	if (newServiceID == NULL) {
		SCTestLog("SCNetworkServiceGetServiceID() failed to get service ID from a service for validation. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	CFArrayAppendValue(newServiceOrder, newServiceID);
	CFRetain(newServiceID);
	my_CFRelease(&newService);

	if (!SCNetworkSetSetServiceOrder(newSet, newServiceOrder)) {
		SCTestLog("SCNetworkSetServiceOrder() failed to set service order in a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCNetworkSetSetCurrent(newSet)) {
		SCTestLog("SCNetworkSetSetCurrent() failed to update the current set. Error: %s", SCErrorString(SCError()));
	}

	/* Retain set ID for validation */
	newSetID = SCNetworkSetGetSetID(newSet);
	if (newSetID == NULL) {
		SCTestLog("Failed to get set ID for a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	CFRetain(newSetID);

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after adding a new set. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Network Set set APIs */

	prefsLocked = NO;
	my_CFRelease(&newSet);

	/* VALIDATE NETWORK SET APIs */
	newSet = SCNetworkSetCopy(self.prefs, newSetID);
	if (newSet == NULL) {
		SCTestLog("SCNetworkSetCreate() validation: SCNetworkSetCopy() failed to get a set for a given set ID.");
		goto done;
	}
	allSets = (__bridge_transfer NSArray *)SCNetworkSetCopyAll(self.prefs);
	if (allSets == nil || !CFArrayContainsValue((__bridge CFArrayRef)allSets,
						    CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)allSets)),
						    newSet)) {
		SCTestLog("SCNetworkSetCreate() validation: SCNetworkSetCopyAll() failed to retrieve new set from prefs.");
		goto done;
	}

	setName = (__bridge NSString *)SCNetworkSetGetName(newSet);
	if ((setName == nil) || !CFEqual((__bridge CFStringRef)setName, kSCTestNetworkSetName)) {
		SCTestLog("SCNetworkSetSetName() validation: SCNetworkSetGetName() failed to get a set name.");
		goto done;
	}

	newService = SCNetworkServiceCopy(self.prefs, newServiceID);
	if (newService == NULL) {
		SCTestLog("SCNetworkServiceCreate() validation: SCNetworkServiceCopy() failed to copy a new service from prefs.");
		goto done;
	}
	services = (__bridge_transfer NSArray *)SCNetworkSetCopyServices(newSet);
	if (services == nil || !CFArrayContainsValue((__bridge CFArrayRef)services,
						     CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)services)),
						     newService)) {
		SCTestLog("SCNetworkSetAddService() validation: SCNetworkSetCopyServices() failed to retrieve service from a new set.");
		goto done;
	}
	my_CFRelease(&newServiceID);
	my_CFRelease(&newSet);

	newSet = SCNetworkSetCopyCurrent(self.prefs);
	if (newSet == NULL) {
		SCTestLog("SCNetworkSetSetCurrent() validation: SCNetworkSetCopyCurrent() failed to retrieve current set from prefs.");
		goto done;
	}

	setID = (__bridge NSString *)SCNetworkSetGetSetID(newSet);
	if (setID == NULL || !CFEqual(newSetID, (__bridge CFStringRef)setID)) {
		SCTestLog("SCNetworkSetSetCurrent() validation: SCNetworkSetGetSetID() failed to get a new set ID from the current set.");
		goto done;
	}

	currentServiceOrder = SCNetworkSetGetServiceOrder(newSet);
	if (currentServiceOrder != NULL) {
		currentServiceOrderCount = CFArrayGetCount(currentServiceOrder);
	}
	if (currentServiceOrderCount != CFArrayGetCount(newServiceOrder)) {
		SCTestLog("SCNetworkSetSetServiceOrder() validation: SCNetworkSetGetServiceOrder() failed to retrieve service order from the current set.");
		goto done;
	}
	for (CFIndex i = 0, n = CFArrayGetCount(newServiceOrder); i < n; i++) {
		serviceID = (__bridge NSString *)CFArrayGetValueAtIndex(newServiceOrder, i);
		if (!CFEqual((__bridge CFStringRef)serviceID, CFArrayGetValueAtIndex(currentServiceOrder, i))) {
		       SCTestLog("SCNetworkSetSetServiceOrder() validation: SCNetworkSetGetServiceOrder() failed to match service order in the current set.");
		       goto done;
		}
	}

	if (!isA_CFType(newSet, SCNetworkSetGetTypeID())) {
		SCTestLog("SCNetworkSetGetTypeID() failed to get a set Type ID");
		goto done;
	}

	if (!SCNetworkSetContainsInterface(newSet, interface)) {
		SCTestLog("SCNetworkSetContainsInterface() failed to verify interface in a set.");
		goto done;
	}

	/* REMOVE SET */
	/* Remove service from a set */
	if (!SCNetworkSetRemoveService(newSet, newService)) {
		SCTestLog("SCNetworkSetRemoveService() failed to remove service from a set. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	my_CFRelease(&newSet);

	/* Remove old set, as current set cannot be removed */
	if (!SCNetworkSetRemove(oldSet)) {
		SCTestLog("SCNetworkSetRemove() failed to remove set. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	if (!SCPreferencesCommitChanges(self.prefs)) {
		SCTestLog("Failed to commit preferences. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	/* Validate service removal from a set */
	newSet = SCNetworkSetCopy(self.prefs, newSetID);
	if (newSet == NULL) {
		SCTestLog("SCNetworkSetRemoveService() validation: SCNetworkSetCopy() failed to retrieve a new set from prefs.");
		goto done;
	}
	services = (__bridge_transfer NSArray *)SCNetworkSetCopyServices(newSet);
	if (CFArrayContainsValue((__bridge CFArrayRef)services,
				 CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)services)),
				 newService)) {
		SCTestLog("SCNetworkSetRemoveService() validation: SCNetworkSetCopyServices() retrieved a removed service from a new set.");
		goto done;
	}
	my_CFRelease(&newSet);

	/* Validate set removal from prefs */
	allSets = (__bridge_transfer NSArray *)SCNetworkSetCopyAll(self.prefs);
	if (CFArrayContainsValue((__bridge CFArrayRef)allSets,
				 CFRangeMake(0, CFArrayGetCount((__bridge CFArrayRef)allSets)),
				 oldSet)) {
		SCTestLog("SCNetworkSetRemove() validation: SCNetworkSetCopyAll() retrieved a removed set from prefs.");
		goto done;
	}
	setID = (__bridge NSString *)SCNetworkSetGetSetID(oldSet);
	if (setID != NULL) {
		newSet = SCNetworkSetCopy(self.prefs, (__bridge CFStringRef)setID);
		my_CFRelease(&oldSet);
		if (newSet != NULL) {

			SCTestLog("SCNetworkSetRemove() validation: SCNetworkSetCopy() returned copy of a removed set from prefs.");
			goto done;
		}
	}

	SCTestLog("Verified that SCNetworkSet APIs behave as expected");
	status = YES;

done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&newServiceID);
	my_CFRelease(&newService);
	my_CFRelease(&newSetID);
	my_CFRelease(&newSet);
	my_CFRelease(&newServiceOrder);
	my_CFRelease(&oldSet);
	my_CFRelease(&interface);
	return status;
}

#pragma mark -
#pragma mark Network Protocol APIs

- (BOOL)unitTestNetworkProtocolConfiguration
{
	NSString *router = nil;
	NSArray *services = nil;
	NSArray *protocols = nil;
	NSMutableDictionary *dnsConfig = nil;
	NSArray<NSString *> *dnsSearchDomains = nil;
	NSArray<NSString *> *dnsServerAddresses = nil;
	NSMutableDictionary *ipv4Config = nil;
	NSArray<NSString *> *ipv4Addresses = nil;
	NSArray<NSString *> *subnetMasks = nil;
	NSMutableArray *configuredProtocolType = nil;
	CFStringRef serviceID = nil;
	SCNetworkServiceRef newService = NULL;
	SCNetworkInterfaceRef interface = NULL;
	SCNetworkProtocolRef protocol = NULL;
	SCNetworkProtocolRef validateProtocol = NULL;
	BOOL prefsLocked = NO;
	BOOL status = NO;

	if (!SCPreferencesLock(self.prefs, true)) {
		return NO;
	}
	prefsLocked = YES;

	services = (__bridge_transfer NSArray *)SCNetworkServiceCopyAll(self.prefs);
	interface = [self copyInterface:services];
	if (interface == NULL) {
		SCTestLog("Failed to find an interface in prefs.");
		goto done;
	}

	/* NETWORK PROTOCOL APIs */
	newService = SCNetworkServiceCreate(self.prefs, interface);
	if (newService == NULL) {
		SCTestLog("SCNetworkServiceCreate() failed to create a new service for a given interface. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	if (!SCNetworkServiceEstablishDefaultConfiguration(newService)) {
		SCTestLog("SCNetworkServiceEstablishDefaultConfiguration() failed to establish default configuration for a new service. Error: %s", SCErrorString(SCError()));
		goto done;
	}

	protocols = (__bridge_transfer NSArray *)SCNetworkServiceCopyProtocols(newService);
	if ([protocols count] == 0) {
		SCTestLog("SCNetworkServiceCopyProtocols failed to copy protocols from a service. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	configuredProtocolType = [[NSMutableArray alloc] init];
	ipv4Config = [[NSMutableDictionary alloc] init];
	dnsConfig = [[NSMutableDictionary alloc] init];

	for (NSUInteger i = 0; i < [protocols count]; i++) {
		protocol = (__bridge SCNetworkProtocolRef)[protocols objectAtIndex:i];
		if (protocol) {
			CFStringRef protocolType = SCNetworkProtocolGetProtocolType(protocol);
			if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv4)) {
				ipv4Addresses = [NSArray arrayWithObject:@"192.168.3.5"];
				subnetMasks = [NSArray arrayWithObject:@"255.255.255.0"];
				router = @"192.168.3.1";

				[ipv4Config setObject:(__bridge NSString*)kSCValNetIPv4ConfigMethodManual forKey:(__bridge NSString*)kSCPropNetIPv4ConfigMethod];
				[ipv4Config setObject:ipv4Addresses forKey:(__bridge NSString *)kSCPropNetIPv4Addresses];
				[ipv4Config setObject:subnetMasks forKey:(__bridge NSString *)kSCPropNetIPv4SubnetMasks];
				[ipv4Config setObject:router forKey:(__bridge NSString *)kSCPropNetIPv4Router];

				/* Set IPv4 protocol configuration */
				if (!SCNetworkProtocolSetConfiguration(protocol, (__bridge CFMutableDictionaryRef)ipv4Config)) {
					SCTestLog("SCNetworkProtocolSetConfiguration() failed to set IPv4 configuration. Error: %s", SCErrorString(SCError()));
					goto done;
				}
				if (!SCNetworkProtocolSetEnabled(protocol, TRUE)) {
					SCTestLog("SCNetworkProtocolSetEnabled() failed to enable protocol type IPv4. Error: %s", SCErrorString(SCError()));
					goto done;
				}
				[configuredProtocolType addObject:(__bridge NSString*)kSCNetworkProtocolTypeIPv4];

			} else if (CFEqual(protocolType, kSCNetworkProtocolTypeDNS)) {
				dnsSearchDomains = @[@"corp.apple.biz", @"euro.apple.biz", @"asia.apple.biz"];
				dnsServerAddresses = @[@"10.10.10.7", @"10.10.10.8", @"10.10.10.9"];

				[dnsConfig setObject:@TEST_DOMAIN forKey:(__bridge NSString *)kSCPropNetDNSDomainName];
				[dnsConfig setObject:dnsSearchDomains forKey:(__bridge NSString *)kSCPropNetDNSSearchDomains];
				[dnsConfig setObject:dnsServerAddresses forKey:(__bridge NSString *)kSCPropNetDNSServerAddresses];

				/* Set DNS configuration */
				if (!SCNetworkProtocolSetConfiguration(protocol, (__bridge CFMutableDictionaryRef)dnsConfig)) {
					SCTestLog("SCNetworkProtocolSetConfiguration() failed to set DNS configuration. Error: %s", SCErrorString(SCError()));
					goto done;
				}
				if (!SCNetworkProtocolSetEnabled(protocol, TRUE)) {
					SCTestLog("SCNetworkProtocolSetEnabled() failed to enable protocol type DNS. Error: %s", SCErrorString(SCError()));
					goto done;
				}
				[configuredProtocolType addObject:(__bridge NSString*)kSCNetworkProtocolTypeDNS];

			}
		}
	}
	/* Retain service id for validation */
	serviceID = SCNetworkServiceGetServiceID(newService);
	if (serviceID == NULL) {
		SCTestLog("SCNetworkServiceGetServiceID() failed to get service ID from a service for network protocol validation.");
		goto done;
	}
	CFRetain(serviceID);

	if (![self commitAndUnlockPreferences]) {
		SCTestLog("Failed to commit preferences, after network protocol configuration. Error: %s", SCErrorString(SCError()));
		goto done;
	}
	/* End of Network Protocols set APIs */

	prefsLocked = NO;
	my_CFRelease(&newService);

	/* VALIDATE NETWORK PROTOCOL APIs */
	newService = SCNetworkServiceCopy(self.prefs, serviceID);
	if (newService) {
		if ([configuredProtocolType containsObject:(__bridge NSString*)kSCNetworkProtocolTypeIPv4]) {
			NSDictionary *currentIPv4Config = nil;
			NSArray *currentIPv4Addresses = nil;
			NSArray *currentSubnetMasks = nil;
			NSString *routerConfig = nil;
			NSString *configMethod;

			validateProtocol = SCNetworkServiceCopyProtocol(newService, kSCNetworkProtocolTypeIPv4);
			currentIPv4Config = (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(validateProtocol);
			configMethod = CFDictionaryGetValue((__bridge CFDictionaryRef) currentIPv4Config, kSCPropNetIPv4ConfigMethod);
			currentIPv4Addresses = (__bridge NSArray *)CFDictionaryGetValue((__bridge CFDictionaryRef) currentIPv4Config, kSCPropNetIPv4Addresses);
			currentSubnetMasks = (__bridge NSArray *)CFDictionaryGetValue((__bridge CFDictionaryRef) currentIPv4Config, kSCPropNetIPv4SubnetMasks);
			routerConfig = CFDictionaryGetValue((__bridge CFDictionaryRef) currentIPv4Config, kSCPropNetIPv4Router);

			if ([configMethod isNotEqualTo:(__bridge NSString *)kSCValNetIPv4ConfigMethodManual] || [currentIPv4Addresses isNotEqualTo:ipv4Addresses] || [currentSubnetMasks isNotEqualTo:subnetMasks] || [routerConfig isNotEqualTo:router]) {
				SCTestLog("SCNetworkProtocolSetConfiguration() validation failed for IPv4 configuration.");
				goto done;
			}

			if (!SCNetworkProtocolGetEnabled(validateProtocol)) {
				SCTestLog("SCNetworkProtocolSetEnabled() validation: SCNetworkProtocolGetEnabled() returned FALSE when protocol type IPv4 is enabled.");
				goto done;
			}
			my_CFRelease(&validateProtocol);
		}
		if ([configuredProtocolType containsObject:(__bridge NSString*)kSCNetworkProtocolTypeDNS]) {
			NSDictionary *currentDNSConfig = nil;
			NSArray *searchDomains = nil;
			NSArray *serverAddresses = nil;
			NSString *domainName;

			validateProtocol = SCNetworkServiceCopyProtocol(newService, kSCNetworkProtocolTypeDNS);
			currentDNSConfig = (__bridge NSDictionary *)SCNetworkProtocolGetConfiguration(validateProtocol);
			domainName = CFDictionaryGetValue((__bridge CFDictionaryRef) currentDNSConfig, kSCPropNetDNSDomainName);
			searchDomains = (__bridge NSArray *)CFDictionaryGetValue((__bridge CFDictionaryRef) currentDNSConfig, kSCPropNetDNSSearchDomains);
			serverAddresses = (__bridge NSArray *)CFDictionaryGetValue((__bridge CFDictionaryRef) currentDNSConfig, kSCPropNetDNSServerAddresses);

			if ([domainName isNotEqualTo:@TEST_DOMAIN] || [searchDomains isNotEqualTo:dnsSearchDomains] || [serverAddresses isNotEqualTo:dnsServerAddresses]) {
				SCTestLog("SCNetworkProtocolSetConfiguration validation failed for DNS configuration.");
				goto done;
			}

			if (!SCNetworkProtocolGetEnabled(validateProtocol)) {
				SCTestLog("SCNetworkProtocolSetEnabled() validation: SCNetworkProtocolGetEnabled() returned FALSE for protocol type DNS is enabled.");
				goto done;
			}

			/* Verify CFTypeID of protocol */
			if (!isA_CFType(validateProtocol, SCNetworkProtocolGetTypeID())) {
				SCTestLog("SCNetworkProtocolGetTypeID() failed to get protocol Type ID");
				goto done;
			}
			my_CFRelease(&validateProtocol);
		}
	} else {
		SCTestLog("SCNetworkServiceCopy() failed to copy a service with configured protocols");
		goto done;
	}

	SCTestLog("Verified that SCNetworkProtocol APIs behave as expected");
	status = YES;
done:
	if (prefsLocked) SCPreferencesUnlock(self.prefs);
	my_CFRelease(&validateProtocol);
	my_CFRelease(&serviceID);
	my_CFRelease(&newService);
	my_CFRelease(&interface);
	return status;
}

@end
#endif // !TARGET_OS_BRIDGE
