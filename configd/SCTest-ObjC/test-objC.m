/*
 *  Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 *  A Objective-C test target to test SC APIs
 *
 *  Created by Sushant Chavan on 4/21/15.
 */

@import Foundation;
@import SystemConfiguration;

#define MY_APP_NAME	CFSTR("SCTestObjC")
#define TARGET_HOST	"www.apple.com"

static void
test_SCNetworkConfiguration()
{
	NSLog(@"\n\n*** SCNetworkConfiguration ***\n\n");
	CFIndex			count;
	CFIndex			idx;
	CFArrayRef		interfaces;
	
	interfaces = SCNetworkInterfaceCopyAll();
	count = CFArrayGetCount(interfaces);
	NSLog(@"Network Interfaces:\n");
	for (idx=0; idx < count; idx++) {
		SCNetworkInterfaceRef intf;
		CFStringRef bsdName;
		
		intf = CFArrayGetValueAtIndex(interfaces, idx);
		bsdName = SCNetworkInterfaceGetBSDName(intf);
		NSLog(@"- %@", bsdName);
	}
	
	CFRelease(interfaces);
}

static void
test_SCDynamicStore()
{
	NSLog(@"\n\n*** SCDynamicStore ***\n\n");
	CFDictionaryRef		dict;
	CFStringRef		intf;
	CFStringRef		key;
	SCDynamicStoreRef	store;
	
	store = SCDynamicStoreCreate(NULL, MY_APP_NAME, NULL, NULL);
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	dict = SCDynamicStoreCopyValue(store, key);
	intf = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface);
	NSLog(@"- Primary Interface is %@\n", intf);
	
	CFRelease(store);
	CFRelease(dict);
	CFRelease(key);
}

static void
test_SCPreferences()
{
	NSLog(@"\n\n*** SCPreferences ***\n\n");
	CFIndex			count;
	CFIndex			idx;
	CFStringRef		model = NULL;
	SCPreferencesRef	prefs;
	CFArrayRef		services;
	
	prefs = SCPreferencesCreate(NULL, MY_APP_NAME, NULL);
	model = SCPreferencesGetValue(prefs, CFSTR("Model"));
	if (model != NULL) {
		NSLog(@"Current model is %@", model);
	}
	
	services = SCNetworkServiceCopyAll(prefs);
	count = CFArrayGetCount(services);
	NSLog(@"Network Services:\n");
	for (idx = 0; idx < count; idx++) {
		SCNetworkServiceRef serv;
		CFStringRef servName;
		
		serv = CFArrayGetValueAtIndex(services, idx);
		servName = SCNetworkServiceGetName(serv);
		NSLog(@"- %@\n", servName);
	}
	
	CFRelease(prefs);
	CFRelease(services);
}

void
test_SCNetworkReachability()
{
	NSLog(@"\n\n*** SCNetworkReachability ***\n\n");
	SCNetworkReachabilityFlags	flags;
	SCNetworkReachabilityRef	target;
	
	target = SCNetworkReachabilityCreateWithName(NULL, TARGET_HOST);
	(void)SCNetworkReachabilityGetFlags(target, &flags);
	NSLog(@"- Reachability flags for "TARGET_HOST": %#x", flags);
	
	CFRelease(target);
}

void
SCTest()
{
	test_SCNetworkConfiguration();
	test_SCNetworkReachability();
	test_SCPreferences();
	test_SCDynamicStore();
}

int main(int argc, const char * argv[]) {
	@autoreleasepool {
		SCTest();
	}
	return 0;
}
