/*
 *  Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 *  A Swift test target to test SC APIs
 *
 *  Created by Sushant Chavan on 4/21/15.
*/

import Foundation
import SystemConfiguration

let target_host = "www.apple.com"
var application = "SCTest-Swift" as CFString

func
test_SCNetworkConfiguration ()
{
	//SCNetworkConfiguration APIs
	NSLog("\n\n*** SCNetworkConfiguration ***\n\n")
	let interfaceArray:[CFArray]
	let count:CFIndex
	var idx:CFIndex
	
	interfaceArray = SCNetworkInterfaceCopyAll() as! [CFArray]
	count = CFArrayGetCount(interfaceArray)
	NSLog("Network Interfaces:")
	for idx = 0; idx < count ; idx++ {
		let interface = interfaceArray[idx]
		if let bsdName? = SCNetworkInterfaceGetBSDName(interface as! SCNetworkInterface) {
			NSLog("- %@", bsdName as String)
		}
	}
}

func
test_SCNetworkReachability ()
{
	//SCNetworkReachability APIs
	NSLog("\n\n*** SCNetworkReachability ***\n\n")
	let target:SCNetworkReachability?
	var flags:SCNetworkReachabilityFlags = SCNetworkReachabilityFlags.allZeros
	
	target = SCNetworkReachabilityCreateWithName(nil, target_host)
	if target == nil {
		NSLog("Error creating target: %s", SCErrorString(SCError()))
		return
	}
	
	SCNetworkReachabilityGetFlags(target!, &flags)
	NSLog("SCNetworkReachability flags for %@ is %#x", String(target_host), flags.rawValue)
}

func
test_SCPreferences ()
{
	//SCPreferences APIs
	NSLog("\n\n*** SCPreferences ***\n\n")
	let prefs:SCPreferences?
	let networkServices:[CFArray]?
	let count:CFIndex
	var idx:CFIndex
	
	prefs = SCPreferencesCreate(nil, application, nil)
	if prefs == nil {
		NSLog("Error creating prefs: %s", SCErrorString(SCError()))
		return
	}
	
	if let model? = SCPreferencesGetValue(prefs!, "Model" as CFString) {
		NSLog("Current system model is %@", model as! String)
	}
	
	networkServices	= SCNetworkServiceCopyAll(prefs!) as? [CFArray]
	if networkServices == nil {
		NSLog("Error retrieving network services", SCErrorString(SCError()))
		return
	}
	
	count = CFArrayGetCount(networkServices)
	NSLog("Network Services:")
	for idx = 0; idx < count ; idx++ {
		let service	= networkServices?[idx]
		if let serviceName? = SCNetworkServiceGetName(service as! SCNetworkService) {
			NSLog("- %@", serviceName as String)
		}
		
	}
}

func
test_SCDynamicStore ()
{
	//SCDynamicStore APIs
	NSLog("\n\n*** SCDynamicStore ***\n\n")
	let key:CFString
	let store:SCDynamicStore?
	let dict:[String:String]?
	let primaryIntf:String?
	
	store = SCDynamicStoreCreate(nil, application, nil, nil)
	if store == nil {
		NSLog("Error creating session: %s", SCErrorString(SCError()))
		return
	}
	
	key =	SCDynamicStoreKeyCreateNetworkGlobalEntity(nil, kSCDynamicStoreDomainState, kSCEntNetIPv4)
	dict =	SCDynamicStoreCopyValue(store, key) as? [String:String]
	primaryIntf = dict?[kSCDynamicStorePropNetPrimaryInterface as String]
	if (primaryIntf != nil) {
		NSLog("Primary interface is %@", primaryIntf!)
	} else {
		NSLog("Primary interface is unavailable")
	}
}

func
my_main ()
{
	test_SCNetworkConfiguration()
	test_SCNetworkReachability()
	test_SCPreferences()
	test_SCDynamicStore()
}

// Run the test
my_main()
